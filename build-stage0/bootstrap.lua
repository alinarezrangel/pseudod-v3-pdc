#!/usr/bin/env lua5.4
---
--- Created by alinarezrangel.
--- DateTime: 13/07/25 05:28 PM
---

local posix = require "posix"
local sexpr = require "sexpr"
local re = require "re"

-- Utility functions
local function file_exists(path)
   local stat = posix.stat(path)
   return stat ~= nil
end

local function ensure_dir(path)
   posix.mkdir(path, "0755")
end

local function read_file(path)
   local file <close> = io.open(path, "r")
   if not file then
      return nil
   end
   local content = file:read("a")
   return content
end

-- S-expr file I/O wrapper
local function read_sexpr_file(path)
   local file <close> = io.open(path, "r")
   if not file then
      return nil
   end

   local result = {}
   while true do
      local datum, err = sexpr.read(file)
      if not datum then
         if err ~= "EOF" then
            error("S-expr parse error: " .. err)
         else
            break
         end
      end
      table.insert(result, datum)
   end

   return result
end

local function write_sexpr_file(path, data)
   local file <close> = io.open(path, "w")
   if not file then
      error("Cannot write to file: " .. path)
   end

   for _, datum in ipairs(data) do
      local ok, err = sexpr.write(file, datum)
      if not ok then
         error("S-expr write error: " .. err)
      end
      file:write("\n")
   end
end

-- Command line parsing
local function parse_args(args)
   local builddir = nil
   local operation = nil
   local params = {}

   local i = 1
   while i <= #args do
      if args[i] == "-B" then
         i = i + 1
         if i <= #args then
            builddir = args[i]
         else
            error("Option -B requires an argument")
         end
      elseif not operation then
         operation = args[i]
      else
         table.insert(params, args[i])
      end
      i = i + 1
   end

   if not builddir then
      error("Build directory (-B) is required")
   end

   if not operation then
      error("Operation (configure/new/partial) is required")
   end

   return builddir, operation, params
end

local DECLARED_VARS = {"RUNPD", "PDC", "PDC_FLAGS", "ENTRY_FILE", "OUTPUT_FILE"}
local IS_VAR_DECLARED = {}
for i = 1, #DECLARED_VARS do
   IS_VAR_DECLARED[DECLARED_VARS[i]] = true
end

-- Configuration management
local function load_vars(builddir)
   local vars_file = builddir .. "/VARS.txt"
   if not file_exists(vars_file) then
      return {}
   end

   local data = read_sexpr_file(vars_file)
   local vars = {}

   for _, pair in ipairs(data) do
      if type(pair) == "table" and #pair == 2 then
         local key = pair[1]
         local value = pair[2]
         if sexpr.is_symbol(key) then
            local keystr = sexpr.symbol_name(key)
            if IS_VAR_DECLARED[keystr] then
               vars[keystr] = value
            end
         end
      end
   end

   return vars
end

local function save_vars(builddir, vars)
   local vars_file = builddir .. "/VARS.txt"
   local data = {}

   for key, value in pairs(vars) do
      if IS_VAR_DECLARED[key] then
         table.insert(data, {sexpr.new_symbol(key), value})
      end
   end

   write_sexpr_file(vars_file, data)
end

local function parse_var_assignments(params)
   local vars = {}

   for _, param in ipairs(params) do
      local key, value = param:match("^([^=]+)=(.*)$")
      if key and value then
         vars[key] = value
      else
         error("Invalid variable assignment: " .. param)
      end
   end

   return vars
end

-- File dependency discovery
local function discover_files(entry_file, visited, graph)
   local toplevel = not visited
   visited = visited or {}
   graph = graph or {}

   if visited[entry_file] or not file_exists(entry_file) then
      graph[entry_file] = graph[entry_file] or {}
      return {}
   end

   visited[entry_file] = true
   local deps = {}

   local content = read_file(entry_file)
   if not content then
      goto END
   end

   for line in content:gmatch("[^\r\n]+") do
      local module_name = line:match("^%s*utilizar%s+([^%s()%[%]{}]+)")
      if module_name then
         deps[#deps + 1] = module_name .. ".pd"
         local module_file = module_name .. ".pd"
         discover_files(module_file, visited, graph)
      end
   end

   ::END::

   graph[entry_file] = deps

   if toplevel then
      local files = {}
      for name in pairs(visited) do
         files[#files + 1] = name
      end
      return files, graph
   end
end

-- File state management
local function get_file_state(path)
   local stat = posix.stat(path)
   if not stat then
      return nil
   end

   return {
      mtime = stat.mtime,
      ino = stat.ino,
      size = stat.size
   }
end

local function load_state(builddir)
   local state_file = builddir .. "/STATE.txt"
   if not file_exists(state_file) then
      return {}
   end

   local data = read_sexpr_file(state_file)
   local state = {}

   for _, entry in ipairs(data) do
      if type(entry) == "table" and #entry == 2 then
         local path = entry[1]
         local file_state = entry[2]
         if type(file_state) == "table" and #file_state == 3 then
            state[path] = {
               mtime = file_state[1],
               ino = file_state[2],
               size = file_state[3]
            }
         end
      end
   end

   return state
end

local function save_state(builddir, state)
   local state_file = builddir .. "/STATE.txt"
   local data = {}

   for path, file_state in pairs(state) do
      table.insert(data, {
         path,
         {file_state.mtime, file_state.ino, file_state.size}
      })
   end

   write_sexpr_file(state_file, data)
end

local function invert_dep_graph(graph)
   local igraph = {}

   for file, deps in pairs(graph) do
      for i = 1, #deps do
         local depfile = deps[i]
         igraph[depfile] = igraph[depfile] or {}
         igraph[depfile][file] = true
      end
      if #deps == 0 then
         igraph[""] = igraph[""] or {}
         igraph[""][file] = true
      end
   end

   return igraph
end

local function check_changed_files(entry_file, files, old_state, graph)
   local changed = {}
   local new_state = {}

   local igraph = invert_dep_graph(graph)
   local path_lens = {[entry_file] = {len = 0, via = nil}}

   -- FIXME: Files that are changed, but only depended upon by unchanged files
   -- should propagate the changedness via at least one path to the entry file.

   local function pathfind_to_entry(file)
      local depended_by = igraph[file] or igraph[""]
      local path_len, path_dep = 1e999, nil
      for dep in pairs(depended_by) do
         local len
         if path_lens[dep] then
            len = path_lens[dep].len
         else
            len = pathfind_to_entry(dep)
         end
         if len and len < path_len then
            path_len = path_lens[dep].len
            path_dep = dep
         end
      end

      if path_dep then
         local extra
         if changed[path_dep] then
            extra = 0 -- Going via a changed file doesn't add to the path
                      -- len. This way rebuilding changed files is prioritized
                      -- over shorter unchanged ones.
         else
            extra = 1
         end
         path_lens[file] = {len = path_len + extra, via = path_dep}
         return path_len
      else
         return nil
      end
   end

   local actual_changed = {}

   for _, file in ipairs(files) do
      local current_state = get_file_state(file)
      if current_state then
         new_state[file] = current_state
         local old = old_state[file]

         if not old or
                 old.mtime ~= current_state.mtime or
                 old.ino ~= current_state.ino or
                 old.size ~= current_state.size then
            changed[file] = true
            table.insert(actual_changed, file)
         end
      end
   end

   local drilled = {}

   local function drill_path(file)
      if file == entry_file then
         return
      end
      if drilled[file] then
         return
      end
      local shortest = path_lens[file]
      if not shortest.via then
         return
      end
      print(string.format("  %s -> %s", file, shortest.via))
      drilled[file] = true
      changed[file] = true
      drill_path(shortest.via)
   end

   for i = 1, #actual_changed do
      local file = actual_changed[i]
      print(string.format("changed %s", file))
      local ok = pathfind_to_entry(file)
      if not ok then
         print(string.format("could not mark %s as changed", file))
      else
         drill_path(file)
      end
   end

   return changed, new_state
end

-- Database manipulation
local function filter_database(db_file, changed_files, output_file)
   if not file_exists(db_file) then
      return
   end

   local data = read_sexpr_file(db_file)
   if not data or #data == 0 then
      return
   end

   local db = data[1]
   if type(db) ~= "table" or #db < 2 or
           not sexpr.is_symbol(db[1]) or sexpr.symbol_name(db[1]) ~= "base-de-datos" then
      return
   end

   local modulos_entry = nil
   for i = 2, #db do
      if type(db[i]) == "table" and #db[i] >= 2 and
              sexpr.is_symbol(db[i][1]) and sexpr.symbol_name(db[i][1]) == "modulos" then
         modulos_entry = db[i]
         break
      end
   end

   if not modulos_entry then
      return
   end

   local filtered_mods = {}
   for i = 2, #modulos_entry do
      local mod = modulos_entry[i]
      if type(mod) == "table" then
         local nombre = nil
         local ruta = nil
         local extension = nil

         for _, field in ipairs(mod) do
            if type(field) == "table" and #field == 2 then
               local key = field[1]
               local value = field[2]
               if sexpr.is_symbol(key) then
                  local key_name = sexpr.symbol_name(key)
                  if key_name == "nombre" then
                     nombre = value
                  elseif key_name == "ruta" then
                     ruta = value
                  elseif key_name == "extension" then
                     extension = value
                  end
               end
            end
         end

         if nombre and ruta and extension then
            local file_path = nombre .. "." .. extension
            if ruta ~= "." then
               file_path = ruta .. "/" .. file_path
            end
            if not changed_files[file_path] then
               table.insert(filtered_mods, mod)
            else
               print(string.format("filtrado %q", file_path))
            end
         end
      end
   end

   local filtered_modulos = {sexpr.new_symbol("modulos")}
   for _, mod in ipairs(filtered_mods) do
      table.insert(filtered_modulos, mod)
   end

   local filtered_db = {sexpr.new_symbol("base-de-datos"), filtered_modulos}
   write_sexpr_file(output_file, {filtered_db})
end

-- Build execution
local SHLIKE_GRAMMAR = re.compile [[

args <- {| ws? (word (ws word)* ws?)? |} ! .
ws <- %s+
word <- {~ (lit / squot / dquot)+ ~}
lit <- [^"'%s]+
squot <- "'" -> '' [^']* "'" -> ''
dquot <- '"' -> '' [^"]* '"' -> ''

]]

local function split_flags(shlike)
   local res = SHLIKE_GRAMMAR:match(shlike)
   if res then
      return res
   else
      error(string.format("invalid syntax in value %q", shlike))
   end
end

local function build_command_array(vars, builddir, is_partial)
   local cmd = {}

   local runpd_cmd = split_flags(vars["RUNPD"] or "")
   for _, arg in ipairs(runpd_cmd) do
      table.insert(cmd, arg)
   end

   table.insert(cmd, vars["PDC"] or "pdc")

   local flags = split_flags(vars["PDC_FLAGS"] or "")
   for _, flag in ipairs(flags) do
      table.insert(cmd, flag)
   end

   table.insert(cmd, (assert(vars["ENTRY_FILE"], "Must set ENTRY_FILE")))

   if is_partial then
      table.insert(cmd, "--cargar-db")
      table.insert(cmd, builddir .. "/partial.sdb")
   end

   table.insert(cmd, "--guardar-db")
   table.insert(cmd, builddir .. "/all.sdb")
   table.insert(cmd, "-o")
   table.insert(cmd, (assert(vars["OUTPUT_FILE"], "Must set OUTPUT_FILE")))

   return cmd
end

local function execute_build(builddir, vars, is_partial)
   local entry_file = assert(vars["ENTRY_FILE"], "Must set ENTRY_FILE")
   if not entry_file then
      error("ENTRY_FILE variable not set")
   end

   local files, graph = discover_files(entry_file)
   local old_state = load_state(builddir)
   local changed_files, new_state = check_changed_files(entry_file, files, old_state, graph)

   if is_partial then
      if not next(changed_files) then
         -- Nothing changed, abort
         print("Nothing changed")
         return
      end

      filter_database(builddir .. "/all.sdb", changed_files, builddir .. "/partial.sdb")
   end

   local cmd = build_command_array(vars, builddir, is_partial)

   local pid = posix.fork()
   if pid == 0 then
      posix.execp(cmd[1], cmd)
      posix._exit(1)
   elseif pid > 0 then
      local _, reason, status = posix.wait(pid)
      if reason ~= "exited" or status ~= 0 then
         error("PDC execution failed")
      end
   else
      error("Fork failed")
   end

   save_state(builddir, new_state)
end

local function show_var(vars, name)
   if not vars[name] then
      print(string.format("unset %s", name))
   else
      print(string.format("%s=%q", name, vars[name]))
   end
end

-- Main function
local function main(args)
   local builddir, operation, params = parse_args(args)

   ensure_dir(builddir)

   if operation == "configure" then
      local vars = load_vars(builddir)

      local show_vars = false
      for i = 1, #params do
         if params[i] == "--show" then
            show_vars = true
         elseif string.sub(params[i], 1, 1) == "-" then
            error(string.format("Invalid argument %s", params[i]))
         end
      end

      if show_vars then
         for i = 1, #DECLARED_VARS do
            show_var(vars, DECLARED_VARS[i])
         end
         return
      end

      local new_vars = parse_var_assignments(params)

      if not next(new_vars) then
         io.stderr:write("Warning: no variables are being set. Specify VAR=VALUE to set vars or `--show` to view set vars\n")
         return
      end

      for key, value in pairs(new_vars) do
         vars[key] = value
      end

      save_vars(builddir, vars)

   elseif operation == "new" then
      local vars = load_vars(builddir)
      execute_build(builddir, vars, false)

   elseif operation == "partial" then
      local vars = load_vars(builddir)
      execute_build(builddir, vars, true)

   else
      error("Unknown operation: " .. operation)
   end
end

-- Entry point
main(arg)
