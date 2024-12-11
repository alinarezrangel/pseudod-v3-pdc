local lpeg = require "lpeg"
local re = require "re"
local sqlite3 = require "lsqlite3"


local VERSION = "pddoc 1.0"


local WORD_CHAR = "^[a-zA-Z_0-9%+%-%*/<>=$~\xc0-\xfc\x80-\xbf]$"
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

local function tokenize_string(str)
   local p = 1
   return tokenize {
      read = function(self, n)
         if p > string.len(str) then
            return nil
         end
         local sub = string.sub(str, p, p + n - 1)
         p = p + n
         return sub
      end,
   }
end

local function rightpad(s, n)
   local l = string.len(s)
   if l >= n then
      return s
   else
      return s .. string.rep(" ", n - l)
   end
end

local function wrap(state, str)
   if state == "string{}" then
      return string.format("{%s}", str)
   elseif state == "string\"\"" then
      return string.format("\"%s\"", str)
   elseif state == "string«»" then
      return string.format("«%s»", str)
   elseif state == "comment" then
      return string.format("[%s]", str)
   else
      return str
   end
end

local grammar = re.compile [[

document <- {| text {} |}

ws <- %s*

text <- {| {:tag: '' -> 'text' :} (textlit / tag)* |}
textlit <- { textlitchr+ / %nl+ }
textlitchr <- [^@{}%nl] / '{' textlit '}'
tag <- {| {:tag: '' -> 'tag' :}
       '@' ws {:name: atom :}
       ( ws {:p: parens :}? ws {:i: inner :}
       / ws {:p: parens :}
       / ! '(' ! '{') |}
value <- tag / parens / atom
atom <- str / bool / num / kw / sym / quote
sym <- {| {:tag: '' -> 'sym' :} symi |}
symi <- {:s: { [a-zA-Z0-9_+*/!?~=<>|%%-]+ } :}
str <- {| {:tag: '' -> 'str' :} '"' {:s: { strchr* } :} '"' |}
strchr <- '\' (["nrt\] / 'x' hex hex / 'u' hex4 / 'U' hex4 hex4) / [^\"]
hex4 <- hex hex hex hex
hex <- [a-fA-F0-9]
bool <- {| {:tag: '' -> 'bool' :} {:v: '#f' 'alse'? '' -> 'false' / '#t' 'rue'? '' -> 'true' :} |}
num <- {| {:tag: '' -> 'num' :} {:n: { '-'? [0-9]+ ('.' [0-9]+)? } :} |}
kw <- {| {:tag: '' -> 'kw' :} '#:' symi |}
parens <- {| {:tag: '' -> 'list' :} '(' (ws value)* ws ')' |}
inner <- innerlit / innernorm
innerlit <- '|{' { (! '}|' .)* } '}|'
innernorm <- '{' text '}'
quote <- {| {:tag: '' -> 'quote' :} "'" {:v: value :} |}

]]

local function parse_scribble(doc)
   local r = grammar:match(doc)
   if r[2] ~= string.len(doc) + 1 then
      error("error cerca de <" .. string.sub(doc, math.max(1, r[2] - 10), r[2] + 20) .. ">")
   else
      return r[1]
   end
end

local KEYWORDS = {
   "variable", "variables", "adquirir", "instancia", "fijar", "a",
   "escribir", "nl", "leer", "si", "finsi", "sino", "mientras",
   "finmientras", "funcion", "finfuncion", "procedimiento",
   "finprocedimiento", "metodo", "finmetodo", "devolver", "llamar",
   "finargs", "con", "de", "y", "e", "clase", "hereda", "extiende",
   "implementa", "finclase", "atributo", "atributos", "estatico",
   "clonar", "finclonar", "son", "sean", "iguales", "diferentes", "y",
   "tanto", "como", "algun", "o", "necesitas", "utilizar", "no",
   "finimplementa", "ref", "identicos", "identicas",
}
local IS_KW = {}
for i = 1, #KEYWORDS do
   IS_KW[KEYWORDS[i]] = true
end

local function create_tokenstream(tokens)
   local tq = {}
   for i = 1, #tokens do
      tokens[i].id = i
      tq[i] = tokens[i]
   end

   local ts = {
      tokens = tokens,
      cur = 1,
   }

   return ts, tq
end

local function peek(ts)
   return ts.tokens[ts.cur]
end

local function consume(ts)
   local t = ts.tokens[ts.cur]
   if ts.cur <= #ts.tokens then
      ts.cur = ts.cur + 1
   end
   return t
end

local function inspect(ts)
   local t = peek(ts)
   if not t then
      print("EOF")
   else
      print(t.state, ("%q"):format(t.string))
   end
end

local function eof(ts)
   return ts.cur > #ts.tokens
end

local function is_comdoc(str)
   local inner = string.match(str, "^DOCUMENTA%s+(.*)%s+DOCUMENTA$")
   if not inner then
      inner = string.match(str, "^DOCUMENTA%s+DOCUMENTA$")
      if inner then
         inner = ""
      end
   end
   return inner
end

local function is_ws(t)
   if t.state == "ws" then
      return true
   elseif t.state == "comment" then
      if is_comdoc(t.string) then
         return false
      else
         return true
      end
   else
      return false
   end
end

local function skip_ws(ts)
   while peek(ts) and is_ws(peek(ts)) do
      consume(ts)
   end
end

local function fail(ts)
   coroutine.yield(false)
end

local function runp(p, ts, ...)
   local coro = coroutine.create(p)
   local ok, res = coroutine.resume(coro, ts, ...)
   if ok then
      if res == false then
         -- print("fail near",
         --       ts.tokens[ts.cur].from_lineno .. ":" .. ts.tokens[ts.cur].from_colno,
         --       ts.tokens[ts.cur].to_lineno .. ":" .. ts.tokens[ts.cur].to_colno,
         --       ts.tokens[ts.cur].state, ("%q"):format(ts.tokens[ts.cur].string))
         return nil
      end
      return res
   else
      io.stderr:write("error: " .. res .. "\n")
      return nil
   end
end

local function fits_name(name, str)
   if not name then
      return true
   elseif type(name) == "string" then
      return name == str
   elseif type(name) == "function" then
      return name(str)
   else
      return false
   end
end

local function expect(ts, kind, name)
   local t = peek(ts)
   if t and kind(t.state) and fits_name(name, t.string) then
      consume(ts)
      return t
   else
      fail(ts)
   end
end

local function expect_word(ts, name)
   return expect(ts, function(k) return k == "word" end, name)
end

local function expect_other(ts, name)
   return expect(ts, function(k) return k == "other" end, name)
end

local function expect_one_of(ts, kinds)
   return expect(
      ts,
      function(k)
         for i = 1, #kinds do
            if kinds[i] == k then
               return true
            end
         end
         return false
      end
   )
end

local function expect_any_word(ts, words)
   return expect(
      ts,
      function(k) return k == "word" end,
      function(n)
         for i = 1, #words do
            if words[i] == n then
               return true
            end
         end
         return false
      end
   )
end

local function is_word(ts)
   local t = peek(ts)
   return t and t.state == "word"
end

local function try_consume(ts, kind, name)
   local t = peek(ts)
   if t and kind(t.state) and fits_name(name, t.string) then
      consume(ts)
      return true
   else
      return false
   end
end

local function try_consume_word(ts, name)
   return try_consume(ts, function(k) return k == "word" end, name)
end

local function try_consume_other(ts, name)
   return try_consume(ts, function(k) return k == "other" end, name)
end

local function p_utilizar(ts)
   skip_ws(ts)
   expect_word(ts, "utilizar")
   skip_ws(ts)
   local tg = expect_one_of(ts, {"word", "string"})
   local ut = {ut = true}
   if tg.state == "word" then
      ut.mod = tg.id
   else
      ut.file = tg.id
   end

   skip_ws(ts)
   if try_consume_word(ts, "como") then
      skip_ws(ts)
      local ns = expect_word(ts)
      ut.ns = ns.id
   end

   skip_ws(ts)
   if try_consume_other(ts, "(") then
      ut.names = {}
      if not try_consume_other(ts, ")") then
         repeat
            local im = {}
            skip_ws(ts)
            local name = expect_word(ts)
            im.orig_name = name.id
            skip_ws(ts)
            if try_consume_word(ts, "como") then
               skip_ws(ts)
               local new_name = expect_word(ts)
               im.local_name = new_name.id
            else
               im.local_name = name.id
            end
            ut.names[#ut.names + 1] = im
            skip_ws(ts)
         until not try_consume_other(ts, ",")
         expect_other(ts, ")")
      end
   end

   return ut
end

local function p_variable(ts)
   skip_ws(ts)
   expect_any_word(ts, {"variable", "variables", "adquirir"})
   local var = {var = true, names = {}}
   repeat
      skip_ws(ts)
      local nm = expect_word(ts)
      var.names[#var.names + 1] = nm.id
      skip_ws(ts)
   until not try_consume_other(ts, ",")
   return var
end

local function p_clase(ts)
   skip_ws(ts)
   expect_word(ts, "clase")

   local cls = {cls = true}

   skip_ws(ts)
   local nm = expect_word(ts)
   cls.name = nm.id

   skip_ws(ts)
   if try_consume_word(ts, "hereda") then
      skip_ws(ts)
      local base = expect_word(ts)
      cls.inh = base.id
   end

   skip_ws(ts)
   while try_consume_word(ts, "implementa") do
      skip_ws(ts)
      local base = expect_word(ts)
      skip_ws(ts)
      cls.impls = cls.impls or {}
      cls.impls[#cls.impls + 1] = base.id
   end

   skip_ws(ts)
   while try_consume_word(ts, "extiende") do
      skip_ws(ts)
      local base = expect_word(ts)
      skip_ws(ts)
      cls.exts = cls.exts or {}
      cls.exts[#cls.exts + 1] = base.id
   end

   return cls
end

local function p_params(ts, m)
   skip_ws(ts)
   if try_consume_other(ts, ":") then
      local params = {}
      repeat
         skip_ws(ts)
         local variadic = false
         if try_consume_other(ts, ".") then
            expect_other(ts, ".")
            expect_other(ts, ".")
            skip_ws(ts)
            variadic = true
         end
         local param = expect_word(ts)
         if variadic then
            params[#params + 1] = {variadic = param.id}
         else
            params[#params + 1] = param.id
         end
         skip_ws(ts)
      until not try_consume_other(ts, ",")
      m.params = params
   else
      m.params = {}
   end
end

local function p_funcion(ts)
   skip_ws(ts)
   local t = expect_any_word(ts, {"funcion", "procedimiento"})
   skip_ws(ts)
   local p = {}
   if t.string == "funcion" then
      p.func = true
   else
      p.proc = true
   end

   local name = expect_word(ts)
   p.name = name.id

   p_params(ts, p)
   return p
end

local function p_metodo(ts)
   skip_ws(ts)
   expect_word(ts, "metodo")
   skip_ws(ts)
   local m, cls, mth
   m = {mth = true}
   local nm1 = expect_word(ts)
   skip_ws(ts)
   if try_consume_other(ts, "#") then
      skip_ws(ts)
      local nm2 = expect_word(ts)
      cls = nm1.id
      mth = nm2.id
   else
      mth = nm1.id
   end

   m.own_cls = cls
   m.name = mth

   p_params(ts, m)
   return m
end

local function p_finclase(ts)
   skip_ws(ts)
   expect_word(ts, "finclase")
   return {finclase = true}
end

local function p_declr(ts, cls_stack)
   skip_ws(ts)
   local t = peek(ts)
   if not t then
      fail(ts)
   elseif t.state ~= "word" then
      fail(ts)
   elseif t.string == "utilizar" then
      return p_utilizar(ts)
   elseif t.string == "variable" or t.string == "variables" or t.string == "adquirir" then
      return p_variable(ts)
   elseif t.string == "clase" then
      local cls = p_clase(ts)
      cls_stack[#cls_stack + 1] = cls.name
      return cls
   elseif t.string == "metodo" then
      local mth = p_metodo(ts)
      if not mth.own_cls and #cls_stack > 0 then
         mth.own_cls = cls_stack[#cls_stack]
      end
      return mth
   elseif t.string == "procedimiento" or t.string == "funcion" then
      return p_funcion(ts)
   else
      fail(ts)
   end
end

local function p_adv(ts, cls_stack)
   skip_ws(ts)
   local t = peek(ts)
   if not t then
      fail(ts)
   elseif t.string == "utilizar" then
      return p_utilizar(ts)
   elseif t.string == "clase" then
      consume(ts)
      skip_ws(ts)
      local name = expect_word(ts)
      cls_stack[#cls_stack + 1] = name.id
      return {cls = true}
   elseif t.string == "finclase" then
      local f = p_finclase(ts)
      cls_stack[#cls_stack] = nil
      return f
   else
      consume(ts)
      return {}
   end
end

local function p_comdoc(ts)
   skip_ws(ts)

   local t = peek(ts)
   if t and t.state == "comment" then
      local inner = is_comdoc(t.string)
      if not inner then
         return { bad = true }
      end
      consume(ts)

      return {
         comdoc = true,
         inner = inner,
      }
   else
      return { bad = true }
   end
end

local function undeclr_comdoc(ast)
   for i = 1, #ast do
      local n = ast[i]
      if type(n) == "table" then
         if n.tag == "tag" and n.name.tag == "sym" and (n.name.s == "modulo" or n.name.s == "toplevel") then
            return true
         end
      end
   end
   return false
end

local function collect(tokens)
   local ts, tq = create_tokenstream(tokens)
   local res, index = {}, {}
   local cls_stack = {}
   while not eof(ts) do
      local comdoc = runp(p_comdoc, ts)
      if comdoc and comdoc.comdoc then
         local ast = parse_scribble(comdoc.inner)
         if undeclr_comdoc(ast) then
            res[#res + 1] = { doc = ast }
         else
            local r = runp(p_declr, ts, cls_stack)
            if r then
               res[#res + 1] = { doc = ast, decl = r }
            else
               res[#res + 1] = { doc = ast }
            end
         end
      else
         r = runp(p_adv, ts, cls_stack)
         if r then
            if r.ut then
               res[#res + 1] = { decl = r }
            end
         else
            consume(ts)
         end
      end

      if eof(ts) then
         break
      end
   end
   return res, tq
end

local function is_tag(t)
   return type(t) == "table"
      and getmetatable(t)
      and getmetatable(t).tag
end

local function make_sym(s)
   return setmetatable({s = s}, { sym = true })
end

local function is_sym(v)
   return type(v) == "table" and getmetatable(v) and getmetatable(v).sym
end

local function get_sym(s)
   assert(is_sym(s))
   return s.s
end

local function make_kw(kw)
   return setmetatable({}, { kw = kw })
end

local function is_kw(obj)
   return type(obj) == "table" and getmetatable(obj) and getmetatable(obj).kw
end

local function get_kw(obj)
   assert(is_kw(obj))
   return getmetatable(obj).kw
end

local function normalize(body)
   local res = {}

   local function subnorm(body)
      for i = 1, #body do
         local el = body[i]
         if type(el) == "table" and not is_tag(el) and not is_sym(el) and not is_kw(el) then
            subnorm(el)
         else
            res[#res + 1] = el
         end
      end
   end

   subnorm(body)
   return res
end

local function make_tag(name, attrs, body)
   return setmetatable({name = name, attrs = attrs, body = normalize(body)}, {tag = true})
end

local function parse_args(name, args, sig)
   local pos, kw, nkw = {}, {}, 0
   local i = 1
   while i <= #args do
      if is_kw(args[i]) then
         local name = get_kw(args[i])
         i = i + 1
         kw[name] = args[i]
         nkw = nkw + 1
      else
         pos[#pos + 1] = args[i]
      end

      i = i + 1
   end

   if type(sig) == "number" then
      assert(#pos == sig and nkw == 0, "expected " .. sig .. " arguments to " .. name)
   else
      local npos, limit, c = string.match(sig, "^([0-9]+)([-+]?)()")
      if not npos or not limit or not c then
         error("bad signature to " .. name)
      end
      npos = tonumber(npos)
      if limit == "+" then
         assert(#pos >= npos, "expected " .. npos .. " arguments or more to " .. name)
      elseif limit == "-" then
         assert(#pos <= npos, "expected " .. npos .. " arguments or fewer to " .. name)
      else
         assert(#pos == npos, "expected " .. npos .. " arguments to " .. name)
      end

      local unproc = {}
      for k, v in pairs(kw) do
         unproc[k] = true
      end
      while true do
         local kwname, opt, nc = string.match(sig, "%s+#:([a-zA-Z0-9_%+%*/!%?~=<>|%%-]+)%s+([_?])()", c)
         if not kwname or not opt or not nc then
            break
         end
         c = nc

         if opt == "_" then
            -- required
            assert(kw[kwname] ~= nil, "expected keyword argument #:" .. kwname)
         end
         unproc[kwname] = nil
      end

      local optkw = string.match(sig, "%s+(%*?)", c)
      if optkw ~= "*" and next(unproc) then
         local kwname = next(unproc)
         error("unexpected keyword argument #:" .. kwname)
      end
   end

   return pos, kw
end

local function make_tagged(lbl, rec)
   return setmetatable(rec, { lbl = lbl })
end

local function is_tagged(v, lbl)
   if lbl then
      return type(v) == "table" and getmetatable(v) and getmetatable(v).lbl == lbl
   else
      return type(v) == "table" and getmetatable(v) and getmetatable(v).lbl
   end
end

local function inner_text(body)
   local res = {}
   for i = 1, #body do
      if type(body) == "string" then
         res[#res + 1] = body[i]
      end
   end
end

local function map(arr, f)
   local res = {}
   for i = 1, #arr do
      res[#res + 1] = f(arr[i])
   end
   return res
end

local function comma_sep(ls)
   local res = {}
   for i = 1, #ls do
      if i > 1 then
         if i ~= #ls then
            res[#res + 1] = ", "
         else
            res[#res + 1] = " y "
         end
      end
      res[#res + 1] = ls[i]
   end
   return res
end

local function syntax_highlight(lang, code, stx_ctx)
   if type(code) == "table" then
      code = table.concat(normalize(code), "")
   end
   stx_ctx = stx_ctx or {}

   local tokens = tokenize_string(code)

   local res = {}
   for i = 1, #tokens do
      local tk = tokens[i]
      if tk.state == "ws" then
         res[#res + 1] = tk.string
      elseif tk.state == "other" then
         res[#res + 1] = make_tag("span", {class = "syn-special"}, {tk.string})
      elseif tk.state == "word" or tk.state == "word\\" then
         if IS_KW[tk.string] then
            res[#res + 1] = make_tag("span", {class = "syn-kw"}, {tk.string})
         else
            local inner, location
            location = stx_ctx[tk.string]
            if location then
               inner = make_tag("a", {href = location.target}, {tk.string})
            else
               inner = tk.string
            end
            res[#res + 1] = make_tag("span", {class = "syn-id"}, {inner})
         end
      elseif tk.state == "comment" then
         res[#res + 1] = make_tag("span", {class = "syn-comment"}, {"[", tk.string, "]"})
      elseif tk.state == "string\"\"" then
         res[#res + 1] = make_tag("span", {class = "syn-string"}, {'"', tk.string, '"'})
      elseif tk.state == "string{}" then
         res[#res + 1] = make_tag("span", {class = "syn-string"}, {"{", tk.string, "}"})
      elseif tk.state == "string«»" then
         res[#res + 1] = make_tag("span", {class = "syn-string"}, {"«", tk.string, "»"})
      else
         res[#res + 1] = tk.string
      end
   end

   return make_tag("code", {class = "lang-" .. lang}, {res})
end

local globals = {}

function globals.list(args)
   local pos, kw = parse_args("list", args, "0+")
   return pos
end

local BLOCKS = {
   div = true,
   section = true,
   hr = true,
   article = true,
   p = true,
}

function globals.para(args)
   local pos, kw = parse_args("para", args, "0+")
   if #pos == 1 and is_tag(pos[1]) and BLOCKS[string.lower(pos[1].name)] then
      return pos[1]
   elseif #pos == 0 then
      return {}
   else
      return make_tag("p", {}, {pos})
   end
end

function globals.toplevel(args)
   local pos, kw = parse_args("modulo", args, "0")
   return {}
end

function globals.modulo(args)
   local pos, kw = parse_args("modulo", args, "0+")
   local exports = {}
   for i = 1, #pos do
      local t = pos[i]
      assert(is_tagged(t, "exporta"), "modulo accepts only @exporta-modulo and @exporta-ids")
      if t.modulo then
         exports[#exports + 1] = make_tag(
            "li", {},
            {"Todo lo exportado por ", syntax_highlight("pseudod", t.modulo)}
         )
      else
         exports[#exports + 1] = make_tag(
            "li", {},
            {"Los identificadores ", comma_sep(map(t.ids, function(id) return syntax_highlight("pseudod", id) end))}
         )
      end
   end
   return make_tag(
      "div", {class = "module-exports"}, {
         make_tag("p", {class = "module-exports--label"}, {"Exporta:"}),
         make_tag("ul", {class = "module-exports--things"}, {
                     exports
         }),
   })
end

globals["exporta-modulo"] = function(args)
   local pos, kw = parse_args("exporta-modulo", args, "1+")
   return make_tagged("exporta", {modulo = pos[1]})
end

globals["exporta-ids"] = function(args)
   local pos, kw = parse_args("exporta-ids", args, "0+")
   return make_tagged("exporta", {ids = pos})
end

function globals.doc(args)
   local pos, kw = parse_args("doc", args, "0+")
   return make_tag("main", {class = "document"}, {pos})
end

function globals.sec(args)
   local pos, kw = parse_args("sec", args, "0+")
   return make_tag("section", {class = "doc element"}, {pos})
end

function globals.bold(args)
   local pos, kw = parse_args("bold", args, "0+")
   return make_tag("b", {}, {pos})
end

function globals.italic(args)
   local pos, kw = parse_args("italic", args, "0+")
   return make_tag("i", {}, {pos})
end

function globals.code(args)
   local pos, kw = parse_args("code", args, "0+")
   return make_tag("code", {class = "lang-none"}, {pos})
end

local function prepare_global_env(env)
   local stx_ctx = getmetatable(env).stx_ctx

   function env.pd(args)
      local pos, kw = parse_args("pd", args, "0+")
      return syntax_highlight("pseudod", pos, stx_ctx)
   end
end

local eval_sexpr

local function eval_tag(tag, env)
   assert(tag.tag == "tag")
   if tag.p or tag.i then
      local fn = eval_sexpr(tag.name, env)
      local args = {}
      if tag.p then
         assert(tag.p.tag == "list")
         for i = 1, #tag.p do
            args[#args + 1] = eval_sexpr(tag.p[i], env)
         end
      end
      if tag.i then
         assert(tag.i.tag == "text")
         for i = 1, #tag.i do
            local el = tag.i[i]
            if type(el) == "string" then
               args[#args + 1] = el
            else
               args[#args + 1] = eval_sexpr(el, env)
            end
         end
      end
      if type(fn) ~= "function" then
         error("cannot call non-function")
      else
         return fn(args)
      end
   else
      return eval_sexpr(tag.name, env)
   end
end

local function eval_list(ls, env)
   assert(ls.tag == "list")
   local fn = eval_sexpr(ls[1], env)
   local args = {}
   for i = 2, #ls do
      args[#args + 1] = eval_sexpr(ls[i], env)
   end
   if type(fn) ~= "function" then
      error("cannot call non-function")
   else
      return fn(args)
   end
end

local function eval_sym(sym, env)
   local name = sym.s
   return assert(env[name], "unknown name " .. name)
end

local L_hex = lpeg.R"09" + lpeg.R"af" + lpeg.R"AF"
local L_hex4 = L_hex * L_hex * L_hex * L_hex
local L_esc = (lpeg.P"\\" / "") * (
   (lpeg.P'"' / '"')
   + (lpeg.P"n" / "\n")
   + (lpeg.P"r" / "\r")
   + (lpeg.P"t" / "\t")
   + (lpeg.P"\\" / "\\")
   + ((lpeg.P"x" / "") * ((L_hex * L_hex) / function(h) return utf8.char(tonumber(h, 16)) end))
   + ((lpeg.P"u" / "") * (L_hex4 / function(h) return utf8.char(tonumber(h, 16)) end))
   + ((lpeg.P"U" / "") * ((L_hex4 * L_hex4) / function(h) return utf8.char(tonumber(h, 16)) end)))
local L_strchr = (L_esc + (lpeg.P(1) - lpeg.S"\\"))^0
local L_repl = lpeg.Cs(L_strchr)

local function quote(sexpr)
   if sexpr.tag == "quote" then
      return {make_sym "quote", quote(sexpr.v)}
   elseif sexpr.tag == "tag" then
      if sexpr.p or sexpr.i then
         local quoted = {}
         quoted[1] = quote(sexpr.name)
         if sexpr.p then
            for i = 1, #sexpr.p do
               quoted[#quoted + 1] = quote(sexpr.p[i])
            end
         end
         if sexpr.i then
            for i = 1, #sexpr.i do
               local el = sexpr.i[i]
               if type(el) == "string" then
                  quoted[#quoted + 1] = el
               else
                  quoted[#quoted + 1] = quote(el)
               end
            end
         end
         return quoted
      else
         return quote(sexpr.name)
      end
   elseif sexpr.tag == "str" then
      return lpeg.match(L_repl, sexpr.s)
   elseif sexpr.tag == "sym" then
      return make_sym(sexpr.s)
   elseif sexpr.tag == "bool" then
      return sexpr.v == "true"
   elseif sexpr.tag == "num" then
      return tonumber(sexpr.n)
   elseif sexpr.tag == "kw" then
      return make_kw(sexpr.s)
   elseif sexpr.tag == "list" then
      local quoted = {}
      for i = 1, #sexpr do
         quoted[i] = quote(sexpr[i])
      end
      return quoted
   end
end

local function eval_quote(qt, env)
   return quote(qt.v)
end

eval_sexpr = function(sexpr, env)
   if sexpr.tag == "tag" then
      return eval_tag(sexpr, env)
   elseif sexpr.tag == "str" then
      return lpeg.match(L_repl, sexpr.s)
   elseif sexpr.tag == "sym" then
      return eval_sym(sexpr, env)
   elseif sexpr.tag == "bool" then
      return sexpr.v == "true"
   elseif sexpr.tag == "num" then
      return tonumber(sexpr.n)
   elseif sexpr.tag == "kw" then
      return make_kw(sexpr.s)
   elseif sexpr.tag == "list" then
      return eval_list(sexpr, env)
   elseif sexpr.tag == "quote" then
      return eval_quote(sexpr, env)
   else
      error("unreachable: sexpr of type " .. sexpr.tag)
   end
end

local function split_paras(text)
   assert(text.tag == "text")
   local acc = {}
   local paras = {}
   for i = 1, #text do
      local el = text[i]
      if type(el) == "string" and string.match(el, "^\n\n+$") then
         if #acc > 0 then
            paras[#paras + 1] = acc
         end
         acc = {}
      else
         acc[#acc + 1] = el
      end
   end
   if #acc > 0 then
      paras[#paras + 1] = acc
   end
   return paras
end

local genlink_cnt = 0
local function genlink()
   genlink_cnt = genlink_cnt + 1
   return "n_" .. genlink_cnt
end

local function codify_sig_params(tq, params, inner, defs)
   if #params > 0 then
      inner[#inner + 1] = make_tag("span", {class = "syn-special"}, {":"})
   end
   for i = 1, #params do
      local p = params[i]
      if i > 1 then
         inner[#inner + 1] = make_tag("span", {class = "syn-special"}, {","})
      end
      inner[#inner + 1] = " "
      local name, target
      target = genlink()
      if type(p) == "table" and p.variadic then
         inner[#inner + 1] = make_tag("span", {class = "syn-special"}, {"..."})
         inner[#inner + 1] = make_tag("span", {class = "syn-id"}, {
                                         make_tag("a", {href = "#" .. target, id = target}, {
                                                     tq[p.variadic].string
                                         })
         })
         name = tq[p.variadic].string
      else
         inner[#inner + 1] = make_tag("span", {class = "syn-id"}, {
                                         make_tag("a", {href = "#" .. target, id = target}, {tq[p].string})
         })
         name = tq[p].string
      end
      defs[name] = {target = "#" .. target, kind = "local"}
   end
end

local function mangle_name(name)
   name = string.gsub(name, "\\", "")
   local function esc(c)
      local b = string.byte(c)
      return string.format("-%02X", b)
   end
   return (string.gsub(name, "([^a-zA-Z0-9_])", esc))
end

local function codify_signature(tq, sig, genlink)
   local inner, defs = {}, {}

   if sig.var then
      if #sig.names > 1 then
         inner[1] = make_tag("span", {class = "syn-kw"}, {"variables"})
      else
         inner[1] = make_tag("span", {class = "syn-kw"}, {"variable"})
      end

      inner[2] = " "

      for i = 1, #sig.names do
         local n = sig.names[i]
         if i > 1 then
            inner[#inner + 1] = make_tag("span", {class = "syn-special"}, {", "})
         end
         local name, target = tq[n].string, "_" .. mangle_name(tq[n].string)
         inner[#inner + 1] = make_tag("span", {class = "syn-id syn-def"}, {
                                         make_tag("a", {href = "#" .. target, id = target}, {name})
         })
         defs[name] = {target = "#" .. target, kind = "global"}
      end
   elseif sig.cls then
      inner[1] = make_tag("span", {class = "syn-kw"}, {"clase"})
      inner[2] = " "
      local name, target = tq[sig.name].string, "_" .. mangle_name(tq[sig.name].string)
      inner[3] = make_tag("span", {class = "syn-id syn-def"}, {
                             make_tag("a", {href = "#" .. target, id = target}, {name})
      })
      defs[name] = {target = "#" .. target, kind = "global"}
      if sig.impls then
         for i = 1, #sig.impls do
            local im = sig.impls[i]
            inner[#inner + 1] = " "
            inner[#inner + 1] = make_tag("span", {class = "syn-kw"}, {"implementa"})
            inner[#inner + 1] = make_tag("span", {class = "syn-id"}, {tq[im].string})
         end
      end
      if sig.exts then
         for i = 1, #sig.exts do
            local im = sig.exts[i]
            inner[#inner + 1] = " "
            inner[#inner + 1] = make_tag("span", {class = "syn-kw"}, {"extiende"})
            inner[#inner + 1] = make_tag("span", {class = "syn-id"}, {tq[im].string})
         end
      end
   elseif sig.mth then
      inner[1] = make_tag("span", {class = "syn-kw"}, {"metodo"})
      inner[2] = " "
      local cls_name = ""
      if sig.own_cls then
         inner[3] = make_tag("span", {class = "syn-id"}, {tq[sig.own_cls].string})
         inner[4] = make_tag("span", {class = "syn-special"}, {"#"})
         cls_name = tq[sig.own_cls].string
      end
      local name, target = tq[sig.name].string, "m_" .. mangle_name(cls_name .. "#" .. tq[sig.name].string)
      inner[#inner + 1] = make_tag("span", {class = "syn-id syn-def"}, {
                                      make_tag("a", {href = "#" .. target, id = target}, {name})
      })
      defs[cls_name .. "#" .. name] = {target = "#" .. target, kind = "global"}
      codify_sig_params(tq, sig.params, inner, defs)
   elseif sig.func or sig.proc then
      inner[1] = make_tag("span", {class = "syn-kw"}, {sig.func and "funcion" or "procedimiento"})
      inner[2] = " "
      local name, target = tq[sig.name].string, "_" .. mangle_name(tq[sig.name].string)
      inner[3] = make_tag("span", {class = "syn-id syn-def"}, {
                             make_tag("a", {href = "#" .. target, id = target}, {name})
      })
      defs[name] = {target = "#" .. target, kind = "global"}
      codify_sig_params(tq, sig.params, inner, defs)
   end

   return make_tag("div", {class = "signature"}, {inner}), defs
end

local get_src

local function defs_of_utilizar(tq, decl, query_mod)
   assert(decl.ut)
   local defs = {}
   local mod
   if decl.mod then
      mod = query_mod("mod", tq[decl.mod].string)
   else
      mod = query_mod("file", tq[decl.file].string)
   end
   if not mod then
      return defs
   end

   if decl.names then
      for i = 1, #decl.names do
         local n = decl.names[i]
         local orig_name = tq[n.orig_name].string
         local src = get_src(mod, orig_name, query_mod)
         if src then
            local src_mod = query_mod("id", src.doc_id)
            defs[tq[n.local_name].string] = {target = src_mod.output_file .. src.target,
                                             doc_id = mod.id,
                                             orig_name = orig_name,
                                             kind = "global"}
         end
      end
   else
      for i = 1, #mod.exports do
         local n = mod.exports[i]
         local src = get_src(mod, n.name, query_mod)
         if src then
            local src_mod = query_mod("id", src.doc_id)
            defs[n.name] = {target = src_mod.output_file .. src.target,
                            doc_id = src.doc_id,
                            orig_name = src.name,
                            kind = "global"}
         end
      end
   end

   return defs
end

local query_mod

local function eval(tq, collected, env, db, prepare_env)
   local function l_query_mod(kind, q)
      return query_mod(db, kind, q)
   end

   local res, globals_defs, collected_tmp = {}, {}, {}
   for i = 1, #collected do
      local assoc = collected[i]
      if assoc.doc then
         local sightml, defs
         if assoc.decl then
            if assoc.decl.ut then
               defs = defs_of_utilizar(tq, assoc.decl, l_query_mod)
            else
               sightml, defs = codify_signature(tq, assoc.decl)
            end
         else
            defs = {}
         end

         local locals_defs = setmetatable({}, {__index = globals_defs})
         for name, data in pairs(defs) do
            if data.kind == "global" then
               globals_defs[name] = data
            else
               assert(data.kind == "local")
               locals_defs[name] = data
            end
         end

         collected_tmp[i] = {
            stx_ctx = locals_defs,
            sightml = sightml,
         }
      elseif assoc.decl and assoc.decl.ut then
         defs = defs_of_utilizar(tq, assoc.decl, l_query_mod)
         for name, data in pairs(defs) do
            if data.kind == "global" then
               globals_defs[name] = data
            end
         end
      end
   end

   for i = 1, #collected do
      local assoc = collected[i]
      local tmp = collected_tmp[i]
      if assoc.doc then
         local subenv = setmetatable({}, {__index = env, stx_ctx = tmp.stx_ctx})
         if prepare_env then
            prepare_env(subenv)
         end

         local paras = split_paras(assoc.doc)
         for i = 1, #paras do
            local els = {}
            for j = 1, #paras[i] do
               if type(paras[i][j]) == "string" then
                  els[j] = paras[i][j]
               else
                  els[j] = eval_sexpr(paras[i][j], subenv)
               end
            end
            paras[i] = globals.para(els)
         end
         if tmp.sightml then
            table.insert(paras, 1, tmp.sightml)
         end
         local doc = globals.sec(paras)
         res[#res + 1] = {doc = doc, decl = assoc.decl}
      elseif assoc.decl then
         res[#res + 1] = {decl = assoc.decl}
      end
   end

   return res, globals_defs
end

local function group_module(evaled)
   local fragments = {}
   for i = 1, #evaled do
      if evaled[i].doc then
         fragments[#fragments + 1] = evaled[i].doc
      end
   end
   return globals.doc(fragments)
end

local function write_doc(out, doc, ind)
   ind = ind or 0
   if type(doc) == "string" then
      local repls = {
         ["<"] = "&lt;",
         [">"] = "&gt;",
         ["&"] = "&amp;",
      }
      out:write((string.gsub(doc, "([<>&])", repls)))
   elseif is_tag(doc) then
      out:write(string.rep(" ", ind))
      out:write("<" .. doc.name)
      if next(doc.attrs) then
         for k, v in pairs(doc.attrs) do
            out:write(" " .. k .. "=" .. string.format("%q", v))
         end
      end
      if #doc.body > 0 then
         out:write ">\n"
         for i = 1, #doc.body do
            local el = doc.body[i]
            if type(doc.body[i - 1]) == "string" and is_tag(el) then
               out:write "\n"
            end
            if type(el) == "string"
               and not string.match(el, "\n+")
               and type(doc.body[i - 1]) ~= "string"
            then
               out:write(string.rep(" ", ind + 4))
            end

            write_doc(out, el, ind + 4)

            if type(el) == "string"
               and string.match(el, "\n+")
               and type(doc.body[i + 1]) == "string" then
               out:write(string.rep(" ", ind + 4))
            end
            if i == #doc.body and type(el) == "string" then
               out:write "\n"
            end
         end
         out:write(string.rep(" ", ind))
         out:write("</" .. doc.name .. ">\n")
      else
         out:write " />\n"
      end
   elseif is_sym(doc) then
      local sym = get_sym(doc)
      out:write("&'" .. sym .. ";")
   elseif is_kw(doc) then
      out:write("&![[#:" .. get_kw(doc) .. "]]")
   elseif type(doc) == "number" then
      out:write(tostring(doc))
   else
      error("unknown datum " .. type(doc))
   end
end

local HTML_SYMS = {
}

local function write_html_attr(out, v)
   local repls = {
      ["<"] = "&lt;",
      [">"] = "&gt;",
      ["&"] = "&amp;",
      ['"'] = "&quot;",
   }
   out:write('"' .. (string.gsub(v, "([<>&\"])", repls)) .. '"')
end

local function write_html(out, doc, ind)
   ind = ind or 0
   if type(doc) == "string" then
      local repls = {
         ["<"] = "&lt;",
         [">"] = "&gt;",
         ["&"] = "&amp;",
         ['"'] = "&quot;",
      }
      out:write((string.gsub(doc, "([<>&\"])", repls)))
   elseif is_tag(doc) then
      out:write("<" .. doc.name)
      if next(doc.attrs) then
         for k, v in pairs(doc.attrs) do
            out:write(" " .. k .. "=")
            write_html_attr(out, v)
         end
      end
      if #doc.body > 0 then
         out:write ">"
         for i = 1, #doc.body do
            local el = doc.body[i]
            write_html(out, el, ind + 4)
         end
         out:write("</" .. doc.name .. ">")
      else
         out:write " />"
      end
   elseif is_sym(doc) then
      local sym = get_sym(doc)
      if HTML_SYMS[sym] then
         out:write("&" .. HTML_SYMS[sym] .. ";")
      else
         error("invalid symbol: " .. sym)
      end
   elseif is_kw(doc) then
      error("unknown datum: keyword")
   elseif type(doc) == "number" then
      out:write(tostring(doc))
   else
      error("unknown datum " .. type(doc))
   end
end

local function string_writer()
   local acc = {}
   local writer = {}

   function writer:write(...)
      for i = 1, select("#", ...) do
         local v = select(i, ...)
         acc[#acc + 1] = v
      end
   end

   function writer:data()
      acc = {table.concat(acc)}
      return acc[1]
   end

   return writer
end


local function prepare_db(name)
   local db = sqlite3.open(name)
   db:exec [[
      create table if not exists docs (
         id integer primary key,
         html text,
         input_file text,
         output_file text not null,
         module text not null
      );

      create table if not exists exports (
         doc_id integer,
         name text not null,
         target_doc_id integer not null,
         target_name text not null,
         target_link text not null
      );
   ]]
   return db
end

local function new_doc(db, doc, input_file, module_name, output_file)
   local prep = db:prepare "select id from docs where module = ?"
   prep:bind(1, module_name)
   local doc_ids_to_delete = {}
   for row in prep:nrows() do
      doc_ids_to_delete[#doc_ids_to_delete + 1] = row.id
   end
   prep:finalize()

   local ids = ""
   for i = 1, #doc_ids_to_delete do
      if i > 1 then
         ids = ids .. ", "
      end
      ids = ids .. doc_ids_to_delete[i]
   end

   db:exec("delete from docs where id in (" .. ids .. ")")
   db:exec("delete from exports where doc_id in (" .. ids .. ")")

   local writer = string_writer()
   write_html(writer, doc)

   prep = db:prepare "insert into docs (html, input_file, module, output_file) values (?, ?, ?, ?)"
   prep:bind(1, writer:data())
   prep:bind(2, input_file)
   prep:bind(3, module_name)
   prep:bind(4, output_file)
   for _ in prep:nrows() do
   end
   local id = prep:last_insert_rowid()
   prep:finalize()
   return id, writer:data()
end

get_src = function(mod, name, query_mod)
   for i = 1, #mod.exports do
      local exp = mod.exports[i]
      if exp.name == name then
         if exp.target_doc_id ~= mod.id then
            -- exported from another module
            local from = query_mod("id", exp.target_doc_id)
            assert(from, "must have a defined target module")
            return get_src(from, exp.target_name, query_mod)
         else
            return { doc_id = mod.id, name = name, target = exp.target_link }
         end
      end
   end
   return false
end

query_mod = function(db, kind, q)
   local prep
   if kind == "id" then
      prep = db:prepare "select * from docs where id = ?"
      prep:bind(1, q)
   elseif kind == "mod" then
      prep = db:prepare "select * from docs where module = ?"
      prep:bind(1, q)
   else
      error("invalid mod query " .. kind)
   end

   for row in prep:nrows() do
      prep:finalize()

      prep = db:prepare "select * from exports where doc_id = ?"
      prep:bind(1, row.id)
      local exports = {}
      for exps in prep:nrows() do
         exports[#exports + 1] = exps
      end
      prep:finalize()

      row.exports = exports
      return row
   end
   prep:finalize()
   return nil
end

local function save_into_db(db, tq, doc_id, frags, defs)
   local function l_query_mod(kind, q)
      return query_mod(db, kind, q)
   end

   for name, location in pairs(defs) do
      local prep = db:prepare "insert into exports (doc_id, name, target_doc_id, target_link, target_name) values (?, ?, ?, ?, ?)"
      prep:bind(1, doc_id)
      prep:bind(2, name)
      if location.doc_id then
         local mod = l_query_mod("id", location.doc_id)
         local src = get_src(mod, location.orig_name, l_query_mod)
         prep:bind(3, src.doc_id)
         prep:bind(4, src.target)
         prep:bind(5, src.name)
      else
         prep:bind(3, doc_id)
         prep:bind(4, location.target)
         prep:bind(5, name)
      end
      for _ in prep:nrows() do
      end
      prep:finalize()
   end
end


local function validate_config(cfg)
   assert(cfg.nombre_del_proyecto, "nombre_del_proyecto debe tener un valor")
end

local function make_config_env()
   local config_env = {
      _VERSION = _VERSION,
      GENDOC_VERSION = VERSION,
      assert = assert,
      dofile = dofile,
      error = error,
      ipairs = ipairs,
      pairs = pairs,
      next = next,
      pcall = pcall,
      print = print,
      rawequal = rawequal,
      rawget = rawget,
      rawlen = rawlen,
      rawset = rawset,
      select = select,
      setmetatable = setmetatable,
      getmetatable = getmetatable,
      tonumber = tonumber,
      tostring = tostring,
      type = type,
      xpcall = xpcall,
      load = function(chunk, chunkname, mode, env)
         if mode ~= nil and mode ~= "t" then
            error("load cannot load binary chunks", 2)
         end
         return load(chunk, chunkname, "t", env)
      end,
      loadfile = function(filename, mode, env)
         if mode ~= nil and mode ~= "t" then
            error("loadfile cannot load binary chunks", 2)
         end
         return loadfile(filename, "t", env)
      end,
      os = {
         getenv = os.getenv,
         date = os.date,
         time = os.time,
      },
   }

   local subenv = {
      string = string,
      table = table,
      utf8 = utf8,
      math = math,
      coroutine = coroutine,
   }
   for k, v in pairs(subenv) do
      config_env[k] = {}
      for sk, sv in pairs(v) do
         config_env[k][sk] = sv
      end
   end

   config_env._G = config_env

   return config_env
end

local function load_config(config_file)
   local cfg
   do
      local handle <close> = io.open(config_file, "rb")
      cfg = handle:read "a"
   end
   local env = make_config_env()

   local chunk, err = load(cfg, config_file, "t", env)
   if not chunk then
      error("syntax error in the config: " .. err)
   end
   local co = coroutine.create(function() chunk() end)
   local ok, err = coroutine.resume(co)
   if not ok then
      error(err)
   end
   if coroutine.status(co) ~= "dead" then
      error("cannot yield from config")
   end

   validate_config(env)

   return env
end

local DEFAULT_TEMPLATE = [=[
<!DOCTYPE html>
<html lang="es">
    <head>
        <meta charset="utf-8" />
        <title>$=nombre_del_modulo$ - $nombre_del_proyecto$</title>

        $if links_css then$
        $for i = 1, #links_css do$
        $local link = links_css[i]$
        <link rel="stylesheet" href="$=link$" type="text/css" />
        $end$
        $end$

        $color_rx = "^#[a-fA-F0-9][a-fA-F0-9][a-fA-F0-9][a-fA-F0-9][a-fA-F0-9][a-fA-F0-9]$"$

        <style>
         $if color_primario then$
         $if color_primario.color then$
         $assert(string.match(color_primario.color, color_rx))$
         $end$
         $if color_primario.contraste then$
         $assert(string.match(color_primario.contraste, color_rx))$
         $end$
         :root {
             --pddoc-primary-color: $=color_primario.color or "#dfdfdf"$;
             --pddoc-primary-contrast-color: $=color_primario.contraste or "#000000"$;
         }
         $end$
        </style>

        $if links_js then$
        $for i = 1, #links_js do$
        $local link = links_js[i]$
        <script defer type="text/javascript" src="$=link$"></script>
        $end$
        $end$

        $=html_final_de_head$
    </head>
    <body>
        <header class="header">
            <div class="header--title-line">
                $if logo then$
                $local function img()$
                <img class="header--logo" src="$=logo.url$" width="$=logo.ancho$" height="$=logo.alto$" />
                $end$

                $if url then$
                <a class="header--logo-container" href="$=url$">$img()$</a>
                $else$
                <span class="header--logo-container">$img()$</span>
                $end$
                $end$
                <h1 class="header--title">$=nombre_del_proyecto$</h1>
            </div>
            <h2 class="header--subtitle">$=nombre_del_modulo$</h2>
        </header>

        <nav class="nav">
            <h2 class="nav--tree-title nav--tree-toc">Tabla de Contenido</h2>
            <ul class="nav--tree">
                $for i = 1, #toc do$
                $local el = toc[i]$
                <li>
                    <a href="$=el.target$">$=el.name$</a>
                </li>
                $end$
            </ul>
        </nav>

        <div class="content">
            $=contenido$
        </div>

        <footer class="footer">
            $=html_pie_de_pagina$
        </footer>

        $=html_final_de_body$
    </body>
</html>
]=]

local TMPL_GRAMMAR = re.compile [[

grammar <- {| fragment (code fragment)* |} {}
fragment <- {[^$]*}
code <- '$' {([^$] / '$$' -> '$')*} '$'

]]

local function interpolate_template(tmpl, env)
   local segments, pos = TMPL_GRAMMAR:match(tmpl)
   if not segments or not pos then
      error("plantilla inválida")
   end
   if pos ~= string.len(tmpl) + 1 then
      error("plantilla inválida cerca de «" .. string.sub(tmpl, math.max(1, pos - 10), pos + 10) .. "»")
   end

   local code = {}
   for i = 1, #segments do
      local seg = segments[i]
      local is_code = i % 2 == 0
      if is_code then
         if string.sub(seg, 1, 1) == "=" then
            code[#code + 1] = string.format(" ;emit(%s); ", string.sub(seg, 2))
         else
            code[#code + 1] = " " .. seg .. " "
         end
      else
         code[#code + 1] = string.format(" ;emit(%q); ", seg)
      end
   end
   code = table.concat(code)

   local res = {}
   local function emit(data)
      if data == nil then
         return
      end
      data = tostring(data)
      res[#res + 1] = data
   end
   env.emit = emit
   local chunk, err = load(code, "template", "t", env)
   if not chunk then
      error("syntax error in the template: " .. err)
   end
   chunk()

   return table.concat(res)
end


local function mangle_module_name(module_name)
   local function esc(c)
      if c == "/" then
         return "--"
      else
         return string.format("-%02X", string.byte(c))
      end
   end
   return (string.gsub(module_name, "([^a-zA-Z0-9_])", esc))
end

local function parse_opts(args)
   local config = {}

   local i = 1

   local function shift(n)
      n = n or 1
      i = i + n
   end

   local function optarg(arg, optname)
      local r = string.match(arg, "^%-%-..-=(.*)$")
      if r then
         return r
      elseif i > #args then
         error("se esperaba un argumento para la opción " .. optname)
      else
         r = args[i]
         shift()
         return r
      end
   end

   local function optopt(arg)
      local r = string.match(arg, "^%-%-(..-)=")
      if r then
         return nil, r
      end
      local r = string.match(arg, "^%-%-(.+)$")
      if r then
         return nil, r
      end
      local r = string.match(arg, "^%-(.+)$")
      if r then
         return r, nil
      end
      return nil, nil
   end

   local function handleopt(arg, long)
      if long == "database" then
         config.database = optarg(arg, long)
      elseif long == "template" then
         config.template_file = optarg(arg, long)
      elseif long == "config" then
         config.config_file = optarg(arg, long)
      elseif long == "output-dir" then
         config.output_dir = optarg(arg, long)
      else
         error("opción desconocida: --" .. long)
      end
   end

   local to_process = {}

   while i <= #args do
      local arg = args[i]
      shift()
      local shorts, long = optopt(arg)
      if shorts then
         for i = 1, string.len(shorts) do
            local opt = string.sub(shorts, i, i)
            if opt == "d" then
               handleopt(arg, "database")
            elseif opt == "T" then
               handleopt(arg, "template")
            elseif opt == "c" then
               handleopt(arg, "config")
            elseif opt == "o" then
               handleopt(arg, "output-dir")
            else
               error("opción desconocida: -" .. opt)
            end
         end
      elseif long then
         handleopt(arg, long)
      else
         local module, input_file, output_file
         module, input_file = string.match(arg, "^([^:]+):(.+)$")
         if not module or not input_file then
            error("sintáxis inválida para el archivo de entrada " .. arg)
         end

         to_process[#to_process + 1] = {
            module = module,
            input_file = input_file,
            output_file = mangle_module_name(module) .. ".html",
         }
      end
   end

   config.to_process = to_process

   if not config.database then
      error("debes especificar la base de datos a usar")
   end

   return config
end



local config = parse_opts({...})

local db = prepare_db(config.database)

for i = 1, #config.to_process do
   local p = config.to_process[i]
   local tokens
   do
      local input_file <close> = io.open(p.input_file, "rb")
      tokens = tokenize(input_file)
   end
   local r, tq = collect(tokens)
   local frags, defs = eval(tq, r, globals, db, prepare_global_env)
   local doc_id, written = new_doc(db, group_module(frags), p.input_file, p.module, p.output_file)
   save_into_db(db, tq, doc_id, frags, defs)
end

db:close()
