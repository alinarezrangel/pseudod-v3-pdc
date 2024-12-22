package.path = "libs/rxi-json/?.lua;" .. package.path

-- 4 dependencias: sqlite3, json, lpeg y luaposix
local sqlite3 = require "lsqlite3"
local json = require "json"
local stat = require "posix.sys.stat"
local unistd = require "posix.unistd"
local libgen = require "posix.libgen"
local errno = require "posix.errno"
local dirent = require "posix.dirent"
local lpeg = require "lpeg"
local re = require "re"

local function read_file(name)
   local h <close> = io.open(name, "rb")
   return h:read "a"
end

local function write_file(name, content)
   local h <close> = io.open(name, "wb")
   h:write(content)
end


------------------------------------------------------------------------
-- Sistema de logging.
------------------------------------------------------------------------

local log_level = "debug"

local log = {}

local LOG_LEVELS = {
   debug = 0,
   info = 1,
   warn = 2,
   error = 3,
}

local CSI = "\x1B["
local COLORS = {
   none           = CSI .. "0m",
   bold           = CSI .. "1m",
   italic         = CSI .. "3m",
   ["fg:black"]   = CSI .. "30m",
   ["fg:red"]     = CSI .. "31m",
   ["fg:green"]   = CSI .. "32m",
   ["fg:yellow"]  = CSI .. "33m",
   ["fg:blue"]    = CSI .. "34m",
   ["fg:magenta"] = CSI .. "35m",
   ["fg:cyan"]    = CSI .. "36m",
   ["fg:white"]   = CSI .. "37m",
   ["bg:black"]   = CSI .. "40m",
   ["bg:red"]     = CSI .. "41m",
   ["bg:green"]   = CSI .. "42m",
   ["bg:yellow"]  = CSI .. "43m",
   ["bg:blue"]    = CSI .. "44m",
   ["bg:magenta"] = CSI .. "45m",
   ["bg:cyan"]    = CSI .. "46m",
   ["bg:white"]   = CSI .. "47m",
}

local function colorcodes(txt)
   local function esc(seq)
      for part in string.gmatch(seq, "([^,]*)") do
         return COLORS[part] or ""
      end
      return ""
   end

   return (string.gsub(txt, "%[([a-zA-Z,:]+)%]", esc)) .. COLORS.none
end

function log.print(level, fmt, ...)
   if LOG_LEVELS[level] < LOG_LEVELS[log_level] then
      return
   end
   print(string.format(colorcodes(fmt), ...))
end

function log.debug(...)
   log.print("debug", ...)
end

function log.info(...)
   log.print("info", ...)
end

function log.warn(...)
   log.print("warn", ...)
end

function log.error(...)
   log.print("error", ...)
end


------------------------------------------------------------------------
-- El sistema de construcción
--
-- Véase "Build Systems a la Carte by Andrey Mokhov, Neil Mitchell and Simon
-- Peyton Jones"
-- (https://www.microsoft.com/en-us/research/publication/build-systems-la-carte/).
--
-- Implementa un sistema de construcción de suspención, con un reconstructor de
-- "verifying traces" que utiliza el hash de Apenwarr (véase
-- https://apenwarr.ca/log/20181113).
--
-- El código fue copiado y modificado de mi proyecto "build.lua":
-- https://alinarezrangel.github.io/build/.
------------------------------------------------------------------------

local HASH_FILE_NOT_FOUND = 1
local HASH_HAS_NO_FILE = 2

local PREPARE_SQL = [[
-- Verifying Traces store
create table if not exists vt (
   id integer primary key,
   st_mtime,
   st_ino,
   st_size,
   st_mode,
   st_uid,
   st_gid,
   filename text not null,
   target boolean
);

create index if not exists vt_by_filename on vt (filename);

insert into vt (id, filename)
values (1, ':file-not-found'),
       (2, ':has-no-file');

-- Hashes of the dependencies of each hash
create table if not exists vt_deps (
   parent_vt_id integer not null,
   dep_vt_id integer not null
);

create index if not exists vt_deps_by_parent on vt_deps (parent_vt_id);
create index if not exists vt_deps_by_dep on vt_deps (dep_vt_id);
]]

local function prepare_db(db)
   db:exec(PREPARE_SQL)
end

local ST_FIELDS = {
   "st_mtime", "st_ino", "st_size", "st_mode", "st_uid", "st_gid",
}

-- Suspending scheduler
local function schedule(rebuilder, db, tasks)
   local done = {}
   local function fetch(target)
      local task = tasks(target)
      if task and not done[target] then
         local new_task = rebuilder(db, target, task)
         new_task(fetch)
      end
   end
   return function(target)
      return fetch(target)
   end
end

local function select_hash(db, id)
   local stmt = db:prepare "select * from vt where id = ?"
   stmt:bind(1, id)
   for row in stmt:nrows() do
      stmt:finalize()
      return row
   end
   stmt:finalize()
end

local function select_hash_by_filename(db, filename)
   local stmt = db:prepare "select * from vt where filename = ? and target = true"
   stmt:bind(1, filename)
   for row in stmt:nrows() do
      stmt:finalize()
      return row
   end
   stmt:finalize()
end

local function query_dependencies_hashes(db, hash)
   local deps_ids = {}
   local stmt = db:prepare "select * from vt_deps where parent_vt_id = ?"
   stmt:bind(1, hash.id)
   for row in stmt:nrows() do
      deps_ids[#deps_ids + 1] = tostring(row.dep_vt_id)
   end
   stmt:finalize()

   local sql = string.format("select * from vt where id in (%s)", table.concat(deps_ids, ", "))
   local deps = {}
   for row in db:nrows(sql) do
      deps[row.filename] = row
   end
   return deps
end

local function insert_hash(db, hash, parent_hash_id)
   if not parent_hash_id then
      local q = select_hash_by_filename(db, hash.filename)
      if q then
         local stmt = db:prepare "delete from vt where id = ?"
         stmt:bind(1, q.id)
         for _ in stmt:nrows() do
         end
         stmt:finalize()

         stmt = db:prepare "delete from vt_deps where parent_vt_id = ?"
         stmt:bind(1, q.id)
         for _ in stmt:nrows() do
         end
         stmt:finalize()
      end
   end

   stmt = db:prepare "insert into vt (filename, target, st_mtime, st_ino, st_size, st_mode, st_uid, st_gid) \
                              values (?,        ?,      ?,        ?,      ?,       ?,       ?,      ?)"
   stmt:bind(1, hash.filename)
   stmt:bind(2, not parent_hash_id)
   for i = 1, #ST_FIELDS do
      stmt:bind(i + 2, hash[ST_FIELDS[i]])
   end
   for _ in stmt:nrows() do
   end
   local inserted_id = stmt:last_insert_rowid()
   stmt:finalize()

   if parent_hash_id then
      stmt = db:prepare "insert into vt_deps (parent_vt_id, dep_vt_id) values (?, ?)"
      stmt:bind(1, parent_hash_id)
      stmt:bind(2, inserted_id)
      for _ in stmt:nrows() do
      end
      stmt:finalize()
   end

   return select_hash(db, inserted_id)
end

local function get_hash(db, filename)
   local res = {
      filename = filename,
   }
   local st, errmsg = stat.stat(filename)
   if not st then
      return select_hash(db, HASH_FILE_NOT_FOUND)
   else
      for i = 1, #ST_FIELDS do
         res[ST_FIELDS[i]] = st[ST_FIELDS[i]]
      end
      return res
   end
end

local function hash_dirty(old_hash, new_hash)
   if
      (new_hash.id == HASH_FILE_NOT_FOUND)
      or (old_hash.id == HASH_HAS_NO_FILE)
      or (old_hash.filename ~= new_hash.filename)
   then
      return true
   end
   for i = 1, #ST_FIELDS do
      if old_hash[ST_FIELDS[i]] ~= new_hash[ST_FIELDS[i]] then
         return true
      end
   end
   return false
end

local function verify_hash(db, target, hash, get_dependency_hash)
   local stored_hash = select_hash_by_filename(db, target)
   if not stored_hash then
      return false
   end
   if hash_dirty(stored_hash, hash) then
      return false
   end
   local deps_by_filename = query_dependencies_hashes(db, stored_hash)
   for filename, old_dep_hash in pairs(deps_by_filename) do
      local new_dep_hash = get_dependency_hash(filename)
      if hash_dirty(old_dep_hash, new_dep_hash) then
         return false
      end
   end
   return true
end

local function record_hash(db, target, hash, new_dep_hashes)
   if hash.id == HASH_FILE_NOT_FOUND or hash.id == HASH_HAS_NO_FILE then
      return
   end

   local got = insert_hash(db, hash)
   for _, dep_hash in pairs(new_dep_hashes) do
      insert_hash(db, dep_hash, got.id)
   end
end

-- Verifying traces rebuilder
local function rebuilder(db, target, task)
   local hash = get_hash(db, target)
   return function(fetch)
      local function get_dependency_hash(dep_target)
         fetch(dep_target)
         return get_hash(db, dep_target)
      end
      if verify_hash(db, target, hash, get_dependency_hash) then
         -- up to date, nothing to do
      else
         local new_dep_hashes = {}
         local function tracking_fetch(dep_target)
            fetch(dep_target)
            local dep_hash = get_hash(db, dep_target)
            new_dep_hashes[dep_target] = dep_hash
         end

         task(tracking_fetch)
         hash = get_hash(db, target)
         record_hash(db, target, hash, new_dep_hashes)
      end
   end
end


------------------------------------------------------------------------
-- Lógica de PseudoD
--
-- La siguiente parte implementa la integración de los aspectos específicos de
-- PseudoD en el sistema de construcción.
------------------------------------------------------------------------

local function normalize_path(path)
   path = string.gsub(path, "/+", "/")
   path = string.gsub(path, "/+$", "")
   local segs = {}
   for path in string.gmatch(path, "([^/]*)") do
      if path == "." then
         -- ignore
      elseif path == ".." then
         segs[#segs] = nil
      else
         segs[#segs + 1] = path
      end
   end
   return table.concat(segs, "/")
end

local function make_relative(path, root)
   root = normalize_path(root)
   path = normalize_path(path)
   if string.sub(path, 1, string.len(root) + 1) == root .. "/" then
      return string.sub(path, string.len(root) + 2)
   else
      return path
   end
end

local PROJECT_MANIFEST = "proyecto.json"

local function find_project_root()
   local cwd = assert(unistd.getcwd())
   local at = cwd
   local relcwd = false
   repeat
      local h <close> = io.open(at .. "/" .. PROJECT_MANIFEST, "rb")
      if h then
         return at, relcwd or "."
      end
      if relcwd then
         relcwd = assert(libgen.basename(at)) .. "/" .. relcwd
      else
         relcwd = assert(libgen.basename(at))
      end
      at = assert(libgen.dirname(at))
   until at == "/"
   local h <close> = io.open("/" .. PROJECT_MANIFEST, "rb")
   if h then
      return "/", (string.match(cwd, "^/*(.*)$"))
   end
   return nil, nil
end

local function match_glob(glob, filepath)
   glob = normalize_path(glob)
   filepath = normalize_path(filepath)

   local function split(path)
      local segs = {}
      for el in string.gmatch(path, "([^/]*)") do
         segs[#segs + 1] = el
      end
      return segs
   end

   local globp = split(glob)
   local pathp = split(filepath)

   local mglob_idx = nil
   for i = 1, #globp do
      if globp[i] == "**" then
         mglob_idx = i
         break
      end
   end

   local matched = true
   local matching = {}

   local function match_seq(i, j, ip, jp)
      if not matched then
         return
      end

      if (j - i) ~= (jp - ip) then
         matched = false
         return
      else
         for w = i, j do
            local m = ip + (w - i)
            local globel = globp[w]
            local pathel = pathp[m]

            local g = string.find(globel, "%*")
            if g then
               local bef, aft = string.sub(globel, 1, g - 1), string.sub(globel, g + 1, -1)
               if string.sub(pathel, 1, string.len(bef)) ~= bef
                  or string.sub(pathel, string.len(pathel) - string.len(aft) + 1, -1) ~= aft
               then
                  matched = false
                  return
               else
                  matching[#matching + 1] = pathel
               end
            elseif globel ~= pathel then
               matched = false
               return
            elseif #matching > 0 then
               matching[#matching + 1] = pathel
            end
         end
      end
   end

   if mglob_idx then
      if #pathp < (#globp - 1) then
         -- sin elementos suficientes
         return nil
      end

      local rem = #globp - (mglob_idx - 1)
      match_seq(1, mglob_idx - 1, 1, mglob_idx - 1)
      table.move(pathp, mglob_idx, #pathp - (rem - 1), #matching + 1, matching)
      match_seq(mglob_idx + 1, #globp, #pathp - (rem - 1) + 1, #pathp)
   else
      match_seq(1, #globp, 1, #pathp)
   end

   if matched then
      return table.concat(matching, "/")
   else
      return nil
   end
end

local function remove_prefix(val, pref)
   if string.sub(val, 1, string.len(pref)) == pref then
      return string.sub(val, string.len(pref) + 1)
   else
      return val
   end
end

local function glob_files(gpath, glob, exts)
   local found = {}

   local function recur(path)
      local files, errmsg = dirent.dir(path)
      if not files then
         error("Leyendo el directorio " .. path .. ": " .. errmsg)
      end
      for i = 1, #files do
         local f = files[i]

         if string.sub(f, 1, 1) ~= "." then
            local st, errmsg = stat.stat(path .. "/" .. f)
            if not st then
               error("Leyendo el directorio " .. path .. ": " .. errmsg)
            end

            if stat.S_ISDIR(st.st_mode) ~= 0 then
               recur(path .. "/" .. f)
            end

            local full = path .. "/" .. f
            local rel = normalize_path(remove_prefix(full, gpath .. "/"))
            local matched = match_glob(glob, rel)
            if matched then
               for j = 1, #exts do
                  local e = exts[j]
                  if string.sub(f, -string.len(e)) == e then
                     found[#found + 1] = { path = full, globbed = matched }
                     break
                  end
               end
            end
         end
      end
   end

   gpath = normalize_path(gpath)
   if string.sub(gpath, 1, 1) == "/" then
      recur(gpath)
   elseif gpath ~= "." and string.sub(gpath, 1, 2) ~= "./" then
      recur("./" .. gpath)
   else
      recur(gpath)
   end

   return found
end

-- Taken from the SemVer 2.0 website: https://semver.org/
local SEMVER_2_GRAMMAR = re.compile [[
semver <- {| core ("-" prerel "+" build / "-" prerel / "+" build)? |} ! .
core <- {:major: {numberid} :} "." {:minor: {numberid} :} "." {:patch: {numberid} :}
prerel <- {:prerel: {| {prerelid} ("." {prerelid})* |} :}
build <- {:build: {| {buildid} ("." {buildid})* |} :}
prerelid <- alphanumid / numberid
buildid <- alphanumid / digits

alphanumid <- nondigit idchars? / idchars nondigit idchars?
idchars <- (digit / nondigit)+
numberid <- "0" / [1-9] [0-9]*
nondigit <- letter / "-"
digits <- digit+
digit <- [0-9]
letter <- [a-zA-Z]
]]

local function parse_version(ver)
   return SEMVER_2_GRAMMAR:match(ver)
end

local function numerical(s)
   return not not string.match(s, "^[0-9]+$")
end

local function version_lt(lhs, rhs)
   local lmajor, lminor, lpatch = tonumber(lhs.major), tonumber(lhs.minor), tonumber(lhs.patch)
   local rmajor, rminor, rpatch = tonumber(rhs.major), tonumber(rhs.minor), tonumber(rhs.patch)
   if lmajor < rmajor then
      return true
   elseif lmajor > rmajor then
      return false
   end
   if lminor < rminor then
      return true
   elseif lminor > rminor then
      return false
   end
   if lpatch < rpatch then
      return true
   elseif lpatch > rpatch then
      return false
   end

   local lprerel = lhs.prerel or {}
   local rprerel = rhs.prerel or {}

   for i = 1, math.min(#lprerel, #rprerel) do
      local lpre = lprerel[i]
      local rpre = rprerel[i]
      -- numbers < non-numbers
      if not numerical(lpre) and numerical(rpre) then
         return false
      elseif numerical(lpre) and not numerical(rpre) then
         return true
      elseif numerical(lpre) and numerical(rpre) then
         local ln, rn = tonumber(lpre), tonumber(rpre)
         if ln < rn then
            return true
         elseif ln > rn then
            return false
         end
      else
         assert(not numerical(lpre))
         assert(not numerical(rpre))
         -- ascii compare
         if lpre < rpre then
            return true
         elseif lpre > rpre then
            return false
         end
      end
   end

   if #lprerel < #rprerel then
      return false
   elseif #lprerel > #rprerel then
      return true
   else
      -- equal
      return false
   end
end

local function valid_pkg_name(name)
   return not not string.match(name, "^[a-zA-Z_][a-zA-Z_0-9%-]*$"), "nombre del paquete inválido"
end

local function parse_pkg_src(src)
   local reg, pkg = string.match(src, "^([a-zA-Z_][a-zA-Z_0-9%-]*):([a-zA-Z_][a-zA-Z_0-9%-]*)$")
   if reg and pkg then
      return { registry = reg, pkg = pkg }
   end
   reg, pkg = string.match(src, "^([a-zA-Z_][a-zA-Z_0-9%-]*):([a-zA-Z_][a-zA-Z_0-9%-]*/[a-zA-Z_][a-zA-Z_0-9%-]*)$")
   if reg and pkg then
      return { registry = reg, pkg = pkg }
   else
      return nil
   end
end

local function valid_pkg_src(src)
   return (not not parse_pkg_src(src)), "fuente del paquete inválida"
end

local function valid_version(ver)
   return not not parse_version(ver), "versión inválida"
end

local function valid_dep_rule(rule)
   return not not string.match(rule, "^[a-zA-Z_][a-zA-Z_0-9%-]*$"),
      "dependencia inválida: se esperaba el nombre de un paquete"
end

local function valid_src_rule(rule)
   return true
end

-- Inspirado por JSON-Schema
local MANIFEST_SCHEMA = {
   type = "object",
   properties = {
      nombre = { type = "string", required = true, validate = valid_pkg_name },
      paquetes = {
         type = "array",
         required = true,
         items = {
            type = "object",
            properties = {
               nombre = { type = "string", required = true, validate = valid_pkg_name },
               version = { type = "string", required = true, validate = valid_version },
               dependencias = {
                  type = "array",
                  default = function() return {} end,
                  items = { type = "string", validate = valid_dep_rule },
               },
               codigo = {
                  type = "array",
                  items = { type = "string", validate = valid_src_rule },
               },
               compilador = {
                  type = "object",
                  default = function() return {} end,
                  properties = {
                     opciones = {
                        type = "array",
                        default = function() return {} end,
                        items = { type = "string" },
                     },
                     experimentos = {
                        type = "array",
                        default = function() return {} end,
                        items = { type = "string" },
                     },
                     id_modulo = { type = "string" },
                  },
               },
               docs = {
                  type = "object",
                  default = function() return {} end,
                  properties = {
                     config = { type = "string" },
                     codigo = {
                        type = "array",
                        items = { type = "string", validate = valid_src_rule },
                     },
                  },
               },
            },
         },
      },
      paquetes_externos = {
         type = "array",
         default = function() return {} end,
         items = {
            type = "object",
            properties = {
               nombre = { type = "string", required = true, validate = valid_pkg_name },
               fuente = { type = "string", required = true, validate = valid_pkg_src },
               version = { type = "string", required = true, validate = valid_version },
            },
         },
      },
      registros = {
         type = "array",
         default = function() return {} end,
         items = {
            type = "object",
            properties = {
               nombre = { type = "string", required = true, validate = valid_pkg_name },
               url = { type = "string", required = true },
            },
         },
      },
   },
}

local function check_schema(schema, data)
   if schema.type == "object" then
      if type(data) ~= "table" then
         return false, "se esperaba un objeto pero se obtuvo " .. type(data)
      end

      if schema.properties then
         for k in pairs(data) do
            if not schema.properties[k] then
               return false, "propiedad inesperada: " .. k
            end
         end

         for pname, pschema in pairs(schema.properties) do
            if pschema.required and data[pname] == nil then
               return false, "la propiedad " .. pname .. " es requerida"
            end

            if data[pname] == nil and pschema.default then
               data[pname] = pschema.default()
            end

            if data[pname] ~= nil then
               local success, err = check_schema(pschema, data[pname])
               if not success then
                  return false, pname .. ": " .. err
               end
            end
         end
      else
         for k in pairs(data) do
            if type(k) ~= "string" then
               return false, "se esperaba un objeto, pero se obtuvo un arreglo"
            end
         end
      end
   elseif schema.type == "array" then
      if type(data) ~= "table" then
         return false, "se esperaba arreglo pero se obtuvo " .. type(data)
      end

      for k in pairs(data) do
         if type(k) == "string" then
            return false, "se esperaba arreglo pero se obtuvo un objeto"
         end
      end

      if schema.items then
         for i = 1, #data do
            local success, err = check_schema(schema.items, data[i])
            if not success then
               return false, "#" .. i .. ": " .. err
            end
         end
      end
   elseif schema.type ~= type(data) then
      return false, "se esperaba un objeto de tipo " .. schema.type .. " pero se obtuvo uno de tipo " .. type(data)
   end

   if schema.validate then
      local ok, err = schema.validate(data)
      if not ok then
         return false, "valor inválido: " .. err
      end
   end

   return true
end

local function validate_manifest(manifest)
   local pkgs, regs = {}, {}

   for i = 1, #manifest.paquetes do
      local paq = manifest.paquetes[i]
      if pkgs[paq.nombre] then
         return false, "paquete " .. paq.nombre .. " declarado 2 o más veces"
      end
      pkgs[paq.nombre] = true
   end

   for i = 1, #manifest.registros do
      local reg = manifest.registros[i]
      if regs[reg.nombre] then
         return false, "registro " .. reg.nombre .. " declarado 2 o más veces"
      end
      regs[reg.nombre] = true
   end

   for i = 1, #manifest.paquetes_externos do
      local paqext = manifest.paquetes_externos[i]
      local src = parse_pkg_src(paqext.fuente)
      if not src then
         return false, "fuente de paquete inválida: " .. paqext.fuente
      end
      if pkgs[paqext.nombre] then
         return false, "paquete " .. paqext.nombre .. " declarado 2 o más veces"
      end
      pkgs[paqext.nombre] = true
      if not regs[src.registry] then
         return false, "fuente " .. paqext.fuente .. " usa un registro no declarado: " .. src.registry
      end
   end

   for i = 1, #manifest.paquetes do
      local paq = manifest.paquetes[i]
      for j = 1, #paq.dependencias do
         local dep = paq.dependencias[j]
         if not pkgs[dep] then
            return false, "dependencia " .. dep .. " no existe (en el paquete " .. paq.nombre .. ")"
         end
      end
   end

   return true
end

local function load_manifest(path)
   local h <close> = io.open(path .. "/" .. PROJECT_MANIFEST, "rb")
   local data = json.decode(h:read "a")
   assert(check_schema(MANIFEST_SCHEMA, data))
   assert(validate_manifest(data))
   return data
end

local PSEUDOD_EXTENSIONS = {".pd", ".pseudod", ".pseudo", ".psd"}

local function locate_package(manifest, pkgname)
   for i = 1, #manifest.paquetes do
      local paq = manifest.paquetes[i]
      if paq.nombre == pkgname then
         return paq
      end
   end
   return nil, "paquete " .. pkgname .. " no encontrado"
end

local function get_source_files_for_package(root, manifest, pkgname)
   local srcs = {}
   local paq = assert(locate_package(manifest, pkgname))
   if paq.codigo then
      for j = 1, #paq.codigo do
         local rule = paq.codigo[j]
         local got = glob_files(root, rule, PSEUDOD_EXTENSIONS)
         table.move(got, 1, #got, #srcs + 1, srcs)
      end
   else
      local got = glob_files(root, paq.nombre .. "/*", PSEUDOD_EXTENSIONS)
      table.move(got, 1, #got, #srcs + 1, srcs)
   end
   return srcs
end

local function get_modname(srcfile)
   local res = string.match(srcfile, "^(.-)%.[^/%.]+$")
   if not res then
      res = srcfile
   end
   return res
end

local function u64_to_bytes(int)
   return ((int & 0xFF00000000000000) >> 56),
      ((int & 0xFF000000000000) >> 48),
      ((int & 0xFF0000000000) >> 40),
      ((int & 0xFF00000000) >> 32),
      ((int & 0xFF000000) >> 24),
      ((int & 0xFF0000) >> 16),
      ((int & 0xFF00) >> 8),
      (int & 0xFF)
end

local function bytes_to_u32(a, b, c, d)
   return ((a or 0) << 24) | ((b or 0) << 16) | ((c or 0) << 8) | (d or 0)
end

local function left_rotate(w, bits)
   bits = bits & (0x20 - 1)
   w = w & 0xFFFFFFFF
   local ones = ((1 << bits) - 1)
   local topmask = ones << (32 - bits)
   local top = (w & topmask) >> (32 - bits)
   local bot = w & ~topmask
   return (bot << bits) | top
end

local function sha1(input)
   -- Sacado de wikipedia https://en.wikipedia.org/wiki/SHA-1#SHA-1_pseudocode
   -- También leí el código de SHA1 de mpeterv:
   -- https://github.com/mpeterv/sha1

   local h0 = 0x67452301
   local h1 = 0xEFCDAB89
   local h2 = 0x98BADCFE
   local h3 = 0x10325476
   local h4 = 0xC3D2E1F0
   local len = 0
   local w = {}

   local string_byte = string.byte
   local rchunk = nil
   local last_chunk = true
   while true do
      local chunk
      if rchunk then
         chunk = rchunk
         rchunk = nil
         last_chunk = false
      elseif not last_chunk then
         break
      else
         chunk = input:read(64)
         if not chunk then
            break
         end
      end

      len = len + #chunk
      if #chunk < 64 then
         local len2 = len - #chunk
         chunk = chunk
            .. string.char(0x80)
            -- len + 1 + 8 I think means: +1 for the 0x80 byte and +8 for the
            -- trailing len.
            .. string.rep(string.char(0), -(len + 1 + 8) % 64)
            .. string.char(u64_to_bytes(len * 8))
         len = len2 + #chunk
         if #chunk > 64 then
            assert(#chunk == 128)
            rchunk = string.sub(chunk, 65, -1)
            chunk = string.sub(chunk, 1, 64)
         else
            last_chunk = false
         end
         assert(#chunk == 64)
      end

      for i = 1, 16 do
         local j = ((i - 1) * 4) + 1
         w[i] = bytes_to_u32(string_byte(chunk, j, j + 3))
      end

      for i = 17, 80 do
         w[i] = left_rotate(w[i - 3] ~ w[i - 8] ~ w[i - 14] ~ w[i - 16], 1)
      end

      local a = h0
      local b = h1
      local c = h2
      local d = h3
      local e = h4

      for i = 1, 80 do
         local f, k
         if i <= 20 then
            f = (b & c) | (((~b) & 0xFFFFFFFF) & d)
            k = 0x5A827999
         elseif i <= 40 then
            f = b ~ c ~ d
            k = 0x6ED9EBA1
         elseif i <= 60 then
            f = (b & c) | (b & d) | (c & d)
            k = 0x8F1BBCDC
         else
            f = b ~ c ~ d
            k = 0xCA62C1D6
         end

         local temp = (left_rotate(a, 5) + f + e + k + w[i]) & 0xFFFFFFFF
         e = d
         d = c
         c = left_rotate(b, 30)
         b = a
         a = temp
      end

      h0 = (h0 + a) & 0xFFFFFFFF
      h1 = (h1 + b) & 0xFFFFFFFF
      h2 = (h2 + c) & 0xFFFFFFFF
      h3 = (h3 + d) & 0xFFFFFFFF
      h4 = (h4 + e) & 0xFFFFFFFF
   end

   return string.format("%08x%08x%08x%08x%08x", h0, h1, h2, h3, h4)
end

local function hash_file(file)
   local h <close> = io.open(file, "rb")
   local size = assert(h:seek("end", 0))
   assert(h:seek("set", 0))
   return sha1(h, size)
end

local function generate_module_id(srcfile)
   local hash = hash_file(srcfile)
   return "pdh" .. hash
end

local DEFAULT_BUILD_DIR = ".pseudod-build"

local function build_tasks(root, relcwd, manifest, builddir)
   -- Esta tabla es innecesaria, pero me gusta como queda el código así, por
   -- separado.
   local pkgsrcs = {}
   for i = 1, #manifest.paquetes do
      local paq = manifest.paquetes[i]
      assert(not pkgsrcs[paq.nombre])
      pkgsrcs[paq.nombre] = get_source_files_for_package(root, manifest, paq.nombre)
   end

   local rules = {}
   for pkgname, srcs in pairs(pkgsrcs) do
      local pkg = assert(locate_package(manifest, pkgname))
      for i = 1, #srcs do
         local src = srcs[i]
         local relsrc = make_relative(src.path, root)
         local modname = get_modname(make_relative(src.globbed, root))
         local outpath = normalize_path(builddir .. "/compilado/" .. pkgname .. "/" .. modname)
         rules[relsrc] = {
            type = "source",
            src = relsrc,
            abs = src.path,
         }
         rules[outpath .. ".c"] = {
            type = "compile-module",
            src = relsrc,
            abs = src.path,
            module_name = modname,
            package_name = pkgname,
            module_id = pkg.compilador.id_modulo or generate_module_id(src.path),
         }
         rules[outpath .. ".bdm.json"] = {
            type = "alias",
            aliases = outpath .. ".c",
         }
         rules[outpath .. ".o"] = {
            type = "c-compile",
            src = outpath .. ".c",
            main = false,
            package_name = pkgname,
         }
         rules[outpath .. ".main.o"] = {
            type = "c-compile",
            src = outpath .. ".c",
            main = true,
            package_name = pkgname,
         }
      end
   end

   return function(target)
      if rules[target] then
         local rule = rules[target]
         if rule.type == "source" then
            return function(fetch)
               log.debug("[fg:green]fuente[none] %s", rule.src)
            end
         elseif rule.type == "compile-module" then
            return function(fetch)
               fetch(rule.src)
               log.debug("[fg:green]compilando [bold]PD[none] %s", rule.src)
            end
         elseif rule.type == "alias" then
            return function(fetch)
               log.debug("[fg:black]alias[none] %s", rule.aliases)
               fetch(rule.aliases)
            end
         elseif rule.type == "c-compile" then
            return function(fetch)
               fetch(rule.src)
               log.debug("[fg:green]compilando [bold]C[none] %s", rule.src)
            end
         else
            error("invalid rule type: " .. rule.type)
         end
      end
   end
end

local function test_tasks(tasks, target)
   local t = tasks(target)
   if not t then
      error("invalid " .. target)
   end
   t(function(target) test_tasks(tasks, target) end)
end


local root, relcwd = find_project_root()
assert(unistd.chdir(root))
local manifest = load_manifest(root)
local tasks = build_tasks(root, relcwd, manifest, DEFAULT_BUILD_DIR)
test_tasks(tasks, ".pseudod-build/compilado/bepd/builtinsImpl.o")

-- local db = sqlite3.open("outputs/build.sqlite3")
-- prepare_db(db)
-- local function tasks(target)
--    if target == "outputs/build-test/1/calc" then
--       return function(fetch)
--          print("=========================================== RUN 1")
--          fetch "outputs/build-test/1/hola"
--          fetch "outputs/build-test/1/mundo"
--          local c1 = read_file "outputs/build-test/1/hola"
--          local c2 = read_file "outputs/build-test/1/mundo"
--          write_file("outputs/build-test/1/calc", c1 .. " " .. c2)
--       end
--    elseif target == "outputs/build-test/1/calc2" then
--       return function(fetch)
--          print("=========================================== RUN 2")
--          fetch "outputs/build-test/1/calc"
--          local c = read_file "outputs/build-test/1/calc"
--          write_file("outputs/build-test/1/calc2", "«" .. c .. "»")
--       end
--    elseif stat.stat(target) then
--       return function(fetch)
--          print("src", target)
--       end
--    end
-- end
-- local build = schedule(rebuilder, db, tasks)
-- build "outputs/build-test/1/calc2"
-- db:close()
