-- Mantén el tokenizador sincronizado con la copia en `scripts/gendoc/gen.lua`
local WORD_CHAR = "^[a-zA-Z_0-9%+%-%*/<>=$~\xc0-\xfc\x80-\xbf']$"
local OTHER_CHAR = "^[.,;:()%%&#]$"

local function tokenize(input_stream)
   local lineno, colno = 1, 0
   local from_lineno, from_colno = 1, 0
   local state = "ws" -- infer | other | ws | word | word\ | comment | string{} | string"" | string«» | string« | string»
   local token = ""
   local tokens = {}

   local function addtk(ns)
      if state ~= "infer" then
         if ((state == "other" or state == "ws") and token ~= "") or (state ~= "other" and state ~= "ws") then
            tokens[#tokens + 1] = {
               string = token,
               state = state,
               from_lineno = from_lineno,
               from_colno = from_colno,
               to_lineno = lineno,
               to_colno = colno
            }
         end
         token = ""
         from_lineno, from_colno = lineno, colno
      end
      state = ns
   end

   while true do
      local c
      if state == "infer" then
         c = token
      else
         c = input_stream:read(1)
         if not c then
            addtk("quit")
            break
         end
      end

      local next_lineno, next_colno = lineno, colno
      if c == "\n" then
         next_lineno, next_colno = lineno + 1, 0
      elseif string.byte(c) & 192 == 128 then
         -- utf8 continuation byte
      else
         next_colno = colno + 1
      end

      if state == "other" or state == "ws" or state == "infer" then
         if string.match(c, "^%s+$") then
            if state == "ws" then
               token = token .. c
            else
               addtk("ws")
               token = c
            end
         elseif c == "[" then
            addtk("comment")
         elseif c == "{" then
            addtk("string{}")
         elseif c == "\xc2" then
            addtk("string«")
         elseif c == "\"" then
            addtk("string\"\"")
         elseif c == "\\" then
            addtk("word\\")
            token = c
         elseif string.match(c, WORD_CHAR) then
            addtk("word")
            token = c
         elseif string.match(c, OTHER_CHAR) then
            addtk("other")
            token = c
         else
            if state == "other" then
               token = token .. c
            else
               addtk("ws")
               token = c
            end
         end
      elseif state == "word" then
         if string.match(c, WORD_CHAR) then
            token = token .. c
         elseif c == "\\" then
            state = "word\\"
            token = token .. c
         else
            addtk("infer")
            token = c
         end
      elseif state == "word\\" then
         if c == "\\" then
            state = "word"
         end
         token = token .. c
      elseif state == "comment" then
         if c == "]" then
            addtk("ws")
         else
            token = token .. c
         end
      elseif state == "string{}" then
         if c == "}" then
            addtk("ws")
         else
            token = token .. c
         end
      elseif state == "string\"\"" then
         if c == "\"" then
            addtk("ws")
         else
            token = token .. c
         end
      elseif state == "string«" then
         if c == "\xab" then
            state = "string«»"
         else
            state = "word"
            token = token .. "\xc2" .. c
         end
      elseif state == "string«»" then
         if c == "\xc2" then
            addtk("string»")
         else
            token = token .. c
         end
      elseif state == "string»" then
         if c == "\xbb" then
            state = "ws"
            token = ""
         else
            state = "string«»"
            local last = (tokens[#tokens] or {}).string or ""
            tokens[#tokens] = nil
            token = last .. token .. "\xc2" .. c
         end
      else
         error("unreachable state: " .. tostring(state))
      end

      lineno, colno = next_lineno, next_colno
   end

   return tokens
end

local function components_of_modname(import)
   local pkgname, modname = string.match(import, "^([^/]+)/(.+)$")
   if pkgname and modname then
      return {
         path = import,
         pkgname = pkgname,
         modname = modname,
      }
   else
      return nil
   end
end

local function get_pseudod_dependencies(filename)
   local tokens
   do
      local h <close> = assert(io.open(filename, "rb"))
      tokens = tokenize(h)
   end

   local mods = {}
   local kw_utilizar = false
   local tk_modname = false
   for i = 1, #tokens do
      local tk = tokens[i]
      if tk.state == "word" then
         if tk.string == "utilizar" then
            kw_utilizar = true
         elseif kw_utilizar then
            mods[tk.string] = true
            kw_utilizar = false
         end
      elseif tk.state == "ws" or tk.state == "comment" then
         -- skip
      else
         kw_utilizar = false
      end
   end

   local ls = {}
   for path in pairs(mods) do
      local comps = components_of_modname(path)
      if comps then
         ls[#ls + 1] = comps
      else
         io.stderr:write(string.format("No se pudo detectar que archivo de está importando en %s: utilizar %s\n", filename, path))
      end
   end

   table.sort(ls, function(a, b) return a.path < b.path end)

   return ls
end

local function mkesc(t)
   return string.gsub(t, "[$# ]", "\\%1")
end

local tool = arg[1]
table.move(arg, 2, #arg, 1, arg)
arg[#arg] = nil

if tool == "rec" then
   local queue, visited = arg, {}
   while #queue > 0 do
      local f = queue[#queue]
      queue[#queue] = nil
      if visited[f] then
         goto CONTINUE
      end

      visited[f] = true
      local deps = get_pseudod_dependencies(f)
      local mkdeps = {}
      for j = 1, #deps do
         local d = deps[j]
         local depf = string.format("%s/%s.pd", d.pkgname, d.modname)
         mkdeps[#mkdeps + 1] = mkesc(depf)
         queue[#queue + 1] = depf
      end
      if #mkdeps > 0 then
         print(string.format("%s: %s", mkesc(f), table.concat(mkdeps, " ")))
      end

      ::CONTINUE::
   end
elseif tool == "direct" then
   assert(#arg == 1, "only 1 file can be passed")
   local deps = get_pseudod_dependencies(arg[1])
   for i = 1, #deps do
      local d = deps[i]
      local depf = string.format("%s/%s.pd", d.pkgname, d.modname)
      deps[i] = string.gsub(depf, ";", "\\;")
   end
   print(table.concat(deps, ";"))
else
   error("unkown tool " .. tool .. ". Use 'rec' or 'direct' instead")
end
