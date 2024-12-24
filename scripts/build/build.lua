-- Las siguientes variables son modificadas por el instalador.
local INSTALL_PREFIX = nil
local UNIX_INSTALL = false

do
   local rel_path
   if UNIX_INSTALL then
      rel_path = "lib/pseudod/lua-libs/?.lua"
   else
      rel_path = "libs/rxi-json/?.lua"
   end
   if INSTALL_PREFIX then
      package.path = INSTALL_PREFIX .. "/" .. rel_path .. ";" .. package.path
   else
      package.path = rel_path .. ";" .. package.path
   end
end

-- 4 dependencias: sqlite3, json, lpeg y luaposix
local sqlite3 = require "lsqlite3"
local json = require "json"
local stat = require "posix.sys.stat"
local wait = require "posix.sys.wait"
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

local function sha1_string(str)
   local inp = {}
   local i = 1
   local string_sub = string.sub
   function inp:read(n)
      if i > #str then
         return nil
      else
         local r = string_sub(str, i, i + (n - 1))
         i = i + n
         return r
      end
   end
   return sha1(inp)
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

local color_level = "auto" -- always | never | auto

local function colorcodes(txt, none)
   local function esc(seq)
      if none then
         return ""
      end
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
   local colors
   if color_level == "always" then
      colors = true
   elseif color_level == "never" then
      colors = false
   else
      assert(color_level == "auto")
      colors = unistd.isatty(unistd.STDIN_FILENO) == 1
   end
   print(string.format(colorcodes(fmt, not colors), ...))
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

local PREPARE_SQL_BUILD_SYS = [[
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
   direct_hash,
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

local function prepare_db_for_build_system(db)
   db:exec(PREPARE_SQL_BUILD_SYS)
end

local ST_FIELDS = {
   "st_mtime", "st_ino", "st_size", "st_mode", "st_uid", "st_gid",
}

-- Suspending scheduler
local function schedule(rebuilder, db, tasks)
   local done = {}
   local function fetch(target)
      local task, extra = tasks(target)
      if task and not done[target] then
         local new_task = rebuilder(db, target, task, extra)
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

   if hash.direct_hash then
      stmt = db:prepare "insert into vt (filename, target, direct_hash) values (?, ?, ?)"
      stmt:bind(1, hash.filename)
      stmt:bind(2, not parent_hash_id)
      stmt:bind(3, hash.direct_hash)
   else
      stmt = db:prepare "insert into vt (filename, target, st_mtime, st_ino, st_size, st_mode, st_uid, st_gid) \
                                 values (?,        ?,      ?,        ?,      ?,       ?,       ?,      ?)"
      stmt:bind(1, hash.filename)
      stmt:bind(2, not parent_hash_id)
      for i = 1, #ST_FIELDS do
         stmt:bind(i + 2, hash[ST_FIELDS[i]])
      end
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

local function get_hash(db, target)
   if string.sub(target, 1, 1) == ":" then
      local def = {
         filename = target,
         direct_hash = true,
      }
      return select_hash_by_filename(db, target) or def
   else
      local res = {
         filename = target,
      }
      local st, errmsg = stat.stat(target)
      if not st then
         return select_hash(db, HASH_FILE_NOT_FOUND)
      else
         for i = 1, #ST_FIELDS do
            res[ST_FIELDS[i]] = st[ST_FIELDS[i]]
         end
         return res
      end
   end
end

local function hash_dirty(old_hash, new_hash)
   if
      (new_hash.id == HASH_FILE_NOT_FOUND)
      or (old_hash.id == HASH_HAS_NO_FILE)
      or (old_hash.filename ~= new_hash.filename)
      or (old_hash.direct_hash ~= new_hash.direct_hash)
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

   return got
end

-- Verifying traces rebuilder
local function rebuilder(db, target, task, hash_cur)
   local hash = (hash_cur or get_hash)(db, target)
   return function(fetch)
      local function get_dependency_hash(dep_target)
         fetch(dep_target)
         return (hash_cur or get_hash)(db, dep_target)
      end
      if verify_hash(db, target, hash, get_dependency_hash) then
         -- up to date, nothing to do
      else
         local new_dep_hashes = {}
         local function tracking_fetch(dep_target)
            fetch(dep_target)
            local dep_hash = (hash_cur or get_hash)(db, dep_target)
            new_dep_hashes[dep_target] = dep_hash
         end

         local hashed = nil
         local function register_hash(hash)
            hashed = hash
         end

         task(tracking_fetch, register_hash)
         hash = (hash_cur or get_hash)(db, target)
         hash = record_hash(db, target, hash, new_dep_hashes) or hash
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

local function split_path_segments(path)
   local segs = {}
   for el in string.gmatch(path, "([^/]*)") do
      segs[#segs + 1] = el
   end
   return segs
end

local function match_glob(glob, filepath)
   glob = normalize_path(glob)
   filepath = normalize_path(filepath)

   local globp = split_path_segments(glob)
   local pathp = split_path_segments(filepath)

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
                        default = function() return {} end,
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
   local filename = path .. "/" .. PROJECT_MANIFEST
   local h <close> = io.open(filename, "rb")
   local data = json.decode(h:read "a")
   assert(check_schema(MANIFEST_SCHEMA, data))
   assert(validate_manifest(data))
   return data, filename
end

local PSEUDOD_EXTENSIONS = {".pd", ".pseudod", ".pseudo", ".psd"}
local PDDOC_EXTENSIONS = {".pd", ".pseudod", ".pseudo", ".psd",
                          ".pddoc", ".scrbl",
                          ".txt",
                          ".html", ".css", ".js"}

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

local function get_docs_files_for_package(root, manifest, pkgname)
   local srcs = {}
   local paq = assert(locate_package(manifest, pkgname))
   for j = 1, #paq.docs.codigo do
      local rule = paq.docs.codigo[j]
      local got = glob_files(root, rule, PDDOC_EXTENSIONS)
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

local function components_of_modname(import)
   local pkgname, modname = string.match(import, "^([^/]+)/(.+)$")
   if pkgname and modname then
      return {
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
      local h <close> = io.open(filename, "rb")
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
            local comps = components_of_modname(tk.string)
            if comps then
               mods[#mods + 1] = comps
            else
               log.warn("[fg:yellow]ADVERTENCIA[none] No se pudo detectar que archivo de está importando en %s: utilizar %s", filename, tk.string)
            end
            kw_utilizar = false
         end
      elseif tk.state == "ws" or tk.state == "comment" then
         -- skip
      else
         kw_utilizar = false
      end
   end

   return mods
end

local DEFAULT_BUILD_DIR = ".pseudod-build"

local PREPARE_SQL_PSEUDOD = [[
create table if not exists config (
   key text primary key,
   value text not null
);
]]

local function prepare_db_for_pseudod(db)
   db:exec(PREPARE_SQL_PSEUDOD)
end

local function find_in_path(path, program)
   for seg in string.gmatch(path, "([^:]*)") do
      if stat.stat(seg .. "/" .. program) then
         return seg .. "/" .. program
      end
   end
   return nil
end

local SHLIKE_GRAMMAR = re.compile [[

args <- {| ws? (word (ws word)* ws?)? |} ! .
ws <- %s+
word <- {~ (lit / squot / dquot)+ ~}
lit <- [^"'%s]+
squot <- "'" -> '' [^']* "'" -> ''
dquot <- '"' -> '' [^"]* '"' -> ''

]]

local function split_shlike_args(shlike)
   local res = SHLIKE_GRAMMAR:match(shlike)
   if res then
      return res
   else
      return nil, "invalid syntax in value " .. shlike
   end
end

local DEFAULT_CONFIG_VALUES = {}

-- Nombre del compilador de PseudoD (el nuevo). Se buscará en el PATH
function DEFAULT_CONFIG_VALUES.pdc_nombre(get, getenv)
   return "pseudod-pdc"
end

-- Ruta completa del ejecutable del compilador nuevo. "" si no se encontró.
function DEFAULT_CONFIG_VALUES.pdc_ejecutable(get, getenv)
   local path = getenv "PATH" or "/bin"
   local pdc_name = get "pdc_nombre"
   local pdc_path = find_in_path(path, pdc_name)
   if not pdc_path then
      log.error("[fg:red][bold]error[none] no se pudo encontrar un compilador de PseudoD.")
      log.error("      Se buscó el ejecutable %q en el PATH: %s", pdc_name, path)
      return ""
   else
      return pdc_path
   end
end

-- Argumentos adicionales para el compilador. Véase `split_shlike_args()`.
function DEFAULT_CONFIG_VALUES.pdc_args_extra(get, getenv)
   return ""
end

-- Nombre del compilador de C a usar. Solo se usará si no hay una variable de
-- entorno `CC`.
function DEFAULT_CONFIG_VALUES.cc_nombre(get, getenv)
   return "cc"
end

-- Ruta al compilador de C a usar. "" si no se encontró ninguno.
function DEFAULT_CONFIG_VALUES.cc_ejecutable(get, getenv)
   local cc = getenv "CC"
   if not cc then
      local path = getenv "PATH" or "/bin"
      local cc_name = get "cc_nombre"
      local cc_path = find_in_path(path, cc_name)
      if not cc_path then
         log.error("[fg:red][bold]error[none] no se pudo encontrar un compilador de C.")
         log.error("      Se buscó el ejecutable %q en el PATH: %s", cc_name, path)
         return ""
      else
         return cc_path
      end
   else
      if string.find(cc, "/") then
         return cc
      else
         local path = getenv "PATH" or "/bin"
         local cc_path = find_in_path(path, cc)
         if not cc_path then
            log.error("[fg:red][bold]error[none] no se pudo encontrar un compilador de C.")
            log.error("      Se buscó el ejecutable %q en el PATH: %s", cc, path)
            return ""
         else
            return cc_path
         end
      end
   end
end

-- CFLAGS
function DEFAULT_CONFIG_VALUES.cc_cflags(get, getenv)
   return getenv "CFLAGS" or ""
end

-- CLIBS
function DEFAULT_CONFIG_VALUES.cc_clibs(get, getenv)
   return getenv "CLIBS" or ""
end

-- Nombre del ejecutable de Lua 5.4 a usar.
function DEFAULT_CONFIG_VALUES.lua_nombre(get, getenv)
   return "lua5.4"
end

-- Ejecutable de Lua 5.4 a usar
function DEFAULT_CONFIG_VALUES.lua_ejecutable(get, getenv)
   local path = getenv "PATH" or "/bin"
   local lua_name = get "lua_nombre"
   local lua_path = find_in_path(path, lua_name)
   if not lua_path then
      log.error("[fg:red][bold]error[none] no se pudo encontrar un intérprete de Lua 5.4.")
      log.error("      Se buscó el ejecutable %q en el PATH: %s", lua_name, path)
      return ""
   else
      return lua_path
   end
end

-- Argumentos adicionales para todos los programas que usan Lua. Véase `split_shlike_args()`.
function DEFAULT_CONFIG_VALUES.lua_args_extra(get, getenv)
   return ""
end

-- Ruta al programa pddoc / gen.lua a usar.
function DEFAULT_CONFIG_VALUES.pddoc_ruta(get, getenv)
   local pd_install_path = INSTALL_PREFIX or ""
   if pd_install_path == "" then
      pd_install_path = "."
   end
   return pd_install_path .. "/scripts/gendoc/gen.lua"
end

local function get_config_key(db, key)
   local stmt = db:prepare "select * from config where key = ?"
   stmt:bind(1, key)
   for row in stmt:nrows() do
      stmt:finalize()
      return row.value
   end
   stmt:finalize()
   return nil
end

local function set_config_key(db, key, value)
   local val = get_config_key(db, key)
   if val then
      local stmt = db:prepare "update config set value = ? where key = ?"
      stmt:bind(1, value)
      stmt:bind(2, key)
      for _ in stmt:nrows() do
      end
      stmt:finalize()
   else
      local stmt = db:prepare "insert into config (key, value) values (?, ?)"
      stmt:bind(1, key)
      stmt:bind(2, value)
      for _ in stmt:nrows() do
      end
      stmt:finalize()
   end
end

local function remove_config_key(db, key)
   local stmt = db:prepare "delete from config where key = ?"
   stmt:bind(1, key)
   for _ in stmt:nrows() do
   end
   stmt:finalize()
end

local function get_all_config_keys(db)
   local stmt = db:prepare "select * from config"
   local res = {}
   for row in stmt:nrows() do
      res[#res + 1] = row
   end
   stmt:finalize()
   return res
end

local function fetch_config_key(db, fetch, key)
   fetch(":[" .. key .. "]")
   return get_config_key(db, key)
end

local function fetch_env_key(fetch, key)
   fetch(":[@" .. key .. "]")
   return os.getenv(key)
end

local function run_config_key(db, fetch, key)
   local res = get_config_key(db, key)
   if res == nil then
      if DEFAULT_CONFIG_VALUES[key] then
         local function get(key)
            return fetch_config_key(db, fetch, key)
         end
         local function getenv(key)
            return fetch_env_key(fetch, key)
         end
         res = DEFAULT_CONFIG_VALUES[key](get, getenv)
      else
         res = ""
      end
      set_config_key(db, key, res)
      return res
   else
      return res
   end
end

local function mkdir_recur(path)
   local segs = split_path_segments(normalize_path(path))
   for i = 1, #segs do
      local total = {}
      table.move(segs, 1, i, 1, total)
      local subpath = table.concat(total, "/")
      local ok, errmsg, errnum = stat.mkdir(subpath)
      if not ok then
         if errnum == errno.EEXIST then
            -- ignore
         else
            error("mkdir -p " .. subpath, errmsg)
         end
      end
   end
end

local function flatten1(tbl)
   local res = {}
   for i = 1, #tbl do
      local el = tbl[i]
      if type(el) == "table" then
         table.move(el, 1, #el, #res + 1, res)
      else
         res[#res + 1] = el
      end
   end
   return res
end

local function run_cmd(db, fetch, invk)
   assert(#invk > 0, "invk debe tener al menos un elemento")
   fetch(invk[1])
   local cmd = invk[1]
   local args = {}
   table.move(invk, 2, #invk, 1, args)

   local cpid = assert(unistd.fork())
   if cpid == 0 then
      -- hijo
      args[0] = cmd
      assert(unistd.exec(cmd, args))
   else
      -- padre
      local cpid, status, code = wait.wait(cpid)
      if cpid then
         return code
      else
         error(status)
      end
   end
end

local function run_rule(db, root, relcwd, manifest, manifest_path, builddir, module_assocs, target, rule)
   if rule.type == "source" then
      return function(fetch)
         log.debug("[italic]código[none] %s", rule.src)
      end
   elseif rule.type == "compile-module" then
      return function(fetch)
         fetch(rule.src)
         local deps = get_pseudod_dependencies(rule.src)
         local load_dbs = {}
         for i = 1, #deps do
            local d = deps[i]
            local src_dep = module_assocs[d.pkgname .. "/" .. d.modname]
            if src_dep then
               local dep_db_file = builddir .. "/compilado/" .. d.pkgname .. "/" .. d.modname .. ".bdm.json"
               load_dbs[#load_dbs + 1] = "--cargar-db"
               load_dbs[#load_dbs + 1] = dep_db_file
               fetch(dep_db_file)
            end
         end

         if rule.depends_on_manifest then
            fetch(manifest_path)
         end

         mkdir_recur((assert(libgen.dirname(target))))

         local pdc_exe = fetch_config_key(db, fetch, "pdc_ejecutable")
         local pdc_extra_args = fetch_config_key(db, fetch, "pdc_args_extra")

         if pdc_exe == "" then
            log.error("[fg:red][bold]error[none] no se encontró un compilador de PseudoD (nuevo) a usar.")
            log.error("      Véase las opciones de configuración pdc_nombre y pdc_ejecutable y el subcomando `pseudod configurar`")
            error("no se pudo invocar al compilador de PseudoD")
         end
         local invk = flatten1 {
            pdc_exe,
            split_shlike_args(pdc_extra_args),
            "--id-modulo", rule.module_id,
            "--paquete", rule.package_name,
            "--modulo", rule.module_name,
            "-o", target,
            "--guardar-db", rule.out_db,
            load_dbs,
         }
         require "fennel"
         local V = require "fennel.view"
         print(V(invk))
         error("bad")

         log.info("[fg:green]compilado->c [bold]pdc[none] %s", rule.src)
      end
   elseif rule.type == "alias" then
      return function(fetch)
         log.debug("[italic]alias[none] %s -> %s", target, rule.aliases)
         fetch(rule.aliases)
      end
   elseif rule.type == "c-compile" then
      return function(fetch)
         fetch(rule.src)
         log.info("[fg:green]compilando [bold]cc[none] %s", rule.src)
         local h <close> = io.open(target, "wb")
         h:write "cc invk"
      end
   elseif rule.type == "pddoc" then
      return function(fetch)
         local lua_exe = fetch_config_key(db, fetch, "lua_ejecutable")
         local lua_extra_args = fetch_config_key(db, fetch, "lua_args_extra")
         local pddoc_path = fetch_config_key(db, fetch, "pddoc_ruta")
         local pddoc_extra_args = fetch_config_key(db, fetch, "pddoc_args_extra")

         if lua_exe == "" then
            log.error("[fg:red][bold]error[none] no se encontró un intérprete de Lua 5.4 a usar.")
            log.error("      Véase las opciones de configuración lua_ejecutable y el subcomando `pseudod configurar`")
            error("no se pudo invocar pddoc")
         end

         if pddoc_path == "" then
            log.error("[fg:red][bold]error[none] no se encontró el programa pddoc / gen.lua a usar.")
            log.error("      Véase las opciones de configuración pddoc_ruta y el subcomando `pseudod configurar`")
            error("no se pudo invocar pddoc")
         end

         local lua_extra, errmsg = split_shlike_args(lua_extra_args)
         if not lua_extra then
            log.error("[fg:red][bold]error[none] valor inválido de la configuración lua_args_extra: %s", errmsg)
            error("no se pudo invocar pddoc")
         end

         local pddoc_extra, errmsg = split_shlike_args(pddoc_extra_args)
         if not pddoc_extra then
            log.error("[fg:red][bold]error[none] valor inválido de la configuración pddoc_args_extra: %s", errmsg)
            error("no se pudo invocar pddoc")
         end

         mkdir_recur((assert(libgen.dirname(target))))
         local invk = flatten1 {
            lua_exe,
            lua_extra,
            pddoc_path,
            "-d", target,
            "-o", rule.output_dir,
            extra_args,
         }

         if rule.depends_on_manifest then
            fetch(manifest_path)
         end

         if rule.config_file then
            invk[#invk + 1] = "-c"
            invk[#invk + 1] = rule.config_file
         end

         for i = 1, #rule.srcs do
            local input = rule.srcs[i]
            local arg = (input.module_name or "") .. ":" .. input.src
            fetch(input.src)
            invk[#invk + 1] = arg
         end

         local code = run_cmd(db, fetch, invk)
         if code ~= 0 then
            log.error("[fg:red][bold]error[none] error en pddoc")
            error("error en pddoc")
         end
      end
   else
      error("invalid rule type: " .. rule.type)
   end
end

local function build_tasks(db, root, relcwd, manifest, manifest_path, builddir)
   -- Esta tabla es innecesaria, pero me gusta como queda el código así, por
   -- separado.
   local pkgsrcs = {}
   for i = 1, #manifest.paquetes do
      local paq = manifest.paquetes[i]
      assert(not pkgsrcs[paq.nombre])
      pkgsrcs[paq.nombre] = {
         code = get_source_files_for_package(root, manifest, paq.nombre),
         docs = get_docs_files_for_package(root, manifest, paq.nombre),
      }
   end

   local rules = {}
   local patt_rules = {}
   local module_assocs = {}

   for pkgname, all_srcs in pairs(pkgsrcs) do
      local pkg = assert(locate_package(manifest, pkgname))
      local pddoc_inputs = {}
      local srcs = all_srcs.code
      local pddoc_srcs = all_srcs.docs

      for i = 1, #pddoc_srcs do
         local src = pddoc_srcs[i]
         pddoc_inputs[#pddoc_inputs + 1] = {
            src = relsrc,
            module_name = false,
         }
      end

      for i = 1, #srcs do
         local src = srcs[i]
         local relsrc = make_relative(src.path, root)
         local modname = get_modname(make_relative(src.globbed, root))
         local outpath = normalize_path(builddir .. "/compilado/" .. pkgname .. "/" .. modname)

         pddoc_inputs[#pddoc_inputs + 1] = {
            src = relsrc,
            module_name = pkgname .. "/" .. modname,
         }

         module_assocs[pkgname .. "/" .. modname] = relsrc

         rules[relsrc] = {
            type = "source",
            src = relsrc,
            abs = src.path,
            module_name = modname,
         }

         rules[outpath .. ".c"] = {
            type = "compile-module",
            src = relsrc,
            abs = src.path,
            module_name = modname,
            package_name = pkgname,
            module_id = pkg.compilador.id_modulo or generate_module_id(src.path),
            depends_on_manifest = not not pkg.compilador.id_modulo,
            out_db = outpath .. ".bdm.json",
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

      rules[builddir .. "/docs/" .. pkgname .. "/db.sqlite3"] = {
         type = "pddoc",
         srcs = pddoc_inputs,
         config_file = pkg.docs.config,
         depends_on_manifest = not not pkg.docs.config,
         output_dir = builddir .. "/docs/" .. pkgname .. "/out/",
      }

      patt_rules[builddir .. "/docs/" .. pkgname .. "/out/*"] = {
         type = "alias",
         aliases = builddir .. "/docs/" .. pkgname .. "/db.sqlite3",
      }
   end

   return function(target)
      local cfg_key = string.match(target, "^:%[([a-zA-Z_][a-zA-Z_0-9]*)%]$")
      if cfg_key then
         return function(fetch, hash)
            local cfg_value = get_config_key(db, cfg_key)
            if not cfg_value then
               log.info("[italic]configuración[none] Usando valor predeterminado para %s", cfg_key)
               cfg_value = run_config_key(db, fetch, cfg_key)
               set_config_key(db, cfg_key, cfg_value)
            end
            hash(sha1_string(cfg_value or ""))
         end, function(_db, target)
            local cfg_value = get_config_key(db, cfg_key)
            return {
               filename = target,
               direct_hash = sha1_string(cfg_value or ""),
            }
         end
      end

      local env_key = string.match(target, "^:%[@([a-zA-Z_][a-zA-Z_0-9]*)%]$")
      if env_key then
         return function(fetch, hash)
            local env_value = os.getenv(env_key)
            log.debug("[italic]configuración[none] Usando variable de entorno %s", env_key)
            hash(sha1_string(("@" .. env_value) or ""))
         end, function(_db, target)
            local env_value = os.getenv(env_key)
            return {
               filename = target,
               direct_hash = sha1_string(("@" .. env_value) or ""),
            }
         end
      end

      if rules[target] then
         local rule = rules[target]
         return run_rule(db, root, relcwd, manifest, manifest_path, builddir, module_assocs, target, rule)
      else
         for glob, rule in pairs(patt_rules) do
            if match_glob(glob, target) then
               return run_rule(db, root, relcwd, manifest, manifest_path, builddir, module_assocs, target, rule)
            end
         end
      end
   end
end


------------------------------------------------------------------------
-- Interfáz del CLI.
------------------------------------------------------------------------

local ok, errmsg, errnum = stat.mkdir(DEFAULT_BUILD_DIR)
if not ok and errnum ~= errno.EEXIST then
   error(errmsg)
end
local db = sqlite3.open(DEFAULT_BUILD_DIR .. "/db.sqlite3")
prepare_db_for_pseudod(db)
prepare_db_for_build_system(db)
local root, relcwd = find_project_root()
assert(unistd.chdir(root))
local manifest, abs_manifest_path = load_manifest(root)
local manifest_path = make_relative(abs_manifest_path, root)
local tasks = build_tasks(db, root, relcwd, manifest, manifest_path, DEFAULT_BUILD_DIR)
local build = schedule(rebuilder, db, tasks)


local GLOBAL_OPTS = {
   log_level = {
      long = "log-level",
      args = 1,
      default = "info",
   },
   color = {
      long = "color",
      args = 1,
      default = "auto",
   },
   verbose = {
      short = "V",
      long = "verbose",
      args = 0,
      default = false,
   },
}

local OPTS_FOR_SUBCOMMAND = {
   compilar = {
      
   },

   documentar = {
      pkgname = {
         short = "p",
         long = "paquete",
         args = 1,
         default = false,
      },
   },

   configurar = {
      show_cfg = {
         long = "mostrar",
         args = 0,
         default = false,
      },
   },
}

for _subcmd, opts in pairs(OPTS_FOR_SUBCOMMAND) do
   for k, v in pairs(GLOBAL_OPTS) do
      opts[k] = v
   end
end

local function apply_global_opts(args)
   if args.log_level then
      log_level = args.log_level
      if not LOG_LEVELS[log_level] then
         log.error("[fg:red][bold]error[none] Nivel de logging inválido: %s", log_level)
         log.error("      Debe ser 'debug', 'info', 'warn' o 'error'.")
         error("Nivel de logging inválido")
      end
   end

   if args.verbose then
      log_level = "debug"
   end

   if args.color then
      color_level = args.color
      if color_level ~= "always" and color_level ~= "never" and color_level ~= "auto" then
         log.error("[fg:red][bold]error[none] Uso de color inválido: %s", color_level)
         log.error("      Debe ser 'always', 'never' o 'auto'.")
         error("Uso de color inválido")
      end
   end
end

local SUBCOMMANDS = {}

function SUBCOMMANDS.ayuda(args)
   print([[Herramienta general de PseudoD.

Subcomandos:

  help       -- Muestra esta ayuda
  ayuda      -- Muestra esta ayuda
  compilar   -- Compila el proyecto actual
  build      -- Alias de `compilar`
  configurar -- Configura el proyecto actual
  configure  -- Alias de `configurar`
  documentar -- Genera la documentación del proyecto actual
  doc        -- Alias de `documentar`
]])
end

function SUBCOMMANDS.compilar(args)
   --build()
end

function SUBCOMMANDS.documentar(args)
   if args.pkgname then
      local found = false
      for i = 1, #manifest.paquetes do
         local pkg = manifest.paquetes[i]
         if pkg.nombre == args.pkgname then
            found = true
            break
         end
      end
      if not found then
         log.error("[fg:red][bold]error[none] El paquete %s no existe", args.pkgname)
         error("paquete inválido")
      else
         build(DEFAULT_BUILD_DIR .. "/docs/" .. args.pkgname .. "/db.sqlite3")
      end
   else
      for i = 1, #manifest.paquetes do
         local pkg = manifest.paquetes[i]
         log.info("Construyendo la documentación de %s", pkg.nombre)
         build(DEFAULT_BUILD_DIR .. "/docs/" .. pkg.nombre .. "/db.sqlite3")
      end
   end
end

function SUBCOMMANDS.configurar(args)
   for i = 1, #args do
      local k, v = string.match(args[i], "^([a-zA-Z_][a-zA-Z_0-9]*)=(.*)$")
      if k and v then
         set_config_key(db, k, v)
      end
      k = string.match(args[i], "^predet:([a-zA-Z_][a-zA-Z_0-9]*)$")
         or string.match(args[i], "^default:([a-zA-Z_][a-zA-Z_0-9]*)$")
      if k then
         remove_config_key(db, k)
      end
   end

   if args.show_cfg then
      local cfgs = get_all_config_keys(db)
      for i = 1, #cfgs do
         local pair = cfgs[i]
         log.info("%s=%s", pair.key, json.encode(pair.value))
      end
   end
end

local SUBCMD_ALIASES = {
   help = "ayuda",
   build = "compilar",
   configure = "configurar",
   doc = "documentar",
}
for alias, aliased in pairs(SUBCMD_ALIASES) do
   SUBCOMMANDS[alias] = SUBCOMMANDS[aliased]
   OPTS_FOR_SUBCOMMAND[alias] = OPTS_FOR_SUBCOMMAND[aliased]
end


local function getopt(args, opts)
   local shortopts, longopts = {}, {}
   local res = {}

   for var, desc in pairs(opts) do
      desc.var = var
      res[var] = desc.default
      if desc.short then
         shortopts[desc.short] = desc
      end
      if desc.long then
         longopts[desc.long] = desc
      end
   end

   local i = 1
   while i <= #args do
      local arg = args[i]
      i = i + 1

      if arg == "-" then
         i = i - 1
         break
      elseif arg == "--" then
         break
      elseif string.sub(arg, 1, 2) == "--" then
         local k, v = string.match(arg, "^%-%-([^=]+)=(.*)$")
         if not k then
            k = string.match(arg, "^%-%-([^=]+)$")
            if not k then
               return nil, "argumento inválido: " .. arg
            end
         end
         local desc = longopts[k]
         if not desc then
            return nil, "opción desconocida: --" .. k
         end
         if desc.args == 0 then
            if v then
               return nil, "opción --" .. k .. " no acepta argumentos"
            end
            res[desc.var] = true
         else
            assert(desc.args == 1)
            if v then
               res[desc.var] = v
            else
               if i > #args then
                  return nil, "se esperaba un valor para la opción --" .. k
               end
               res[desc.var] = args[i]
               i = i + 1
            end
         end
      elseif string.sub(arg, 1, 1) == "-" then
         for i = 2, string.len(arg) do
            local k = string.sub(arg, i, i)
            local desc = shortopts[k]
            if not desc then
               return nil, "opción desconocida: -" .. k
            end
            if desc.args == 0 then
               res[desc.var] = true
            else
               assert(desc.args == 1)
               if i > #args then
                  return nil, "se esperaba un valor para la opción -" .. k
               end
               res[desc.var] = args[i]
               i = i + 1
            end
         end
      else
         i = i - 1
         break
      end
   end

   local pos = {}
   table.move(args, i, #args, 1, pos)

   return {
      keys = res,
      pos = pos,
   }
end

local function run_subcmd(subcmd, cli_values)
   apply_global_opts(cli_values)
   assert(SUBCOMMANDS[subcmd])(cli_values)
end

local cli = {...}

local res, errmsg = getopt(cli, GLOBAL_OPTS)
if not res then
   log.error("[fg:red][bold]error[none] %s", errmsg)
   error(errmsg)
end
local subcmd = res.pos[1]
local subcmd_cli = {}
if subcmd then
   table.move(res.pos, 2, #res.pos, 1, subcmd_cli)
   local opts = OPTS_FOR_SUBCOMMAND[subcmd] or {}
   if not SUBCOMMANDS[subcmd] then
      log.error("[fg:red][bold]error[none] Subcomando '%s' inválido", subcmd)
      error("subcomando " .. subcmd .. " es inválido")
   end
   local cli_values = res.keys
   res, errmsg = getopt(subcmd_cli, opts)
   if not res then
      log.error("[fg:red][bold]error[none] %s", errmsg)
      error(errmsg)
   end

   for k, v in pairs(res.keys) do
      cli_values[k] = v
   end
   table.move(res.pos, 1, #res.pos, 1, cli_values)
   run_subcmd(subcmd, cli_values)
else
   run_subcmd("help", res.keys)
end

db:close()
