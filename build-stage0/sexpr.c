#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SEXPR_SYMBOL_METATABLE "sexpr.symbol"

// Symbol userdata structure
typedef struct
{
    size_t len;
    char name[];
} sexpr_symbol_t;

// Reader state
typedef struct
{
    lua_State *L;
    int reader_ref;
    int current_char;
    int has_char;  // 1 if current_char has a valid character, 0 if we need to read a new one
    int eof;
} reader_state_t;

// Forward declarations
static int read_datum(reader_state_t *rs);
static int write_datum(lua_State *L, int writer_ref, int datum_idx);

// Utility functions
static int is_whitespace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int is_symbol_char(int c)
{
    return isalnum(c) || c == '.' || c == '?' || c == '!' || c == '+' ||
        c == '*' || c == '/' || c == '_' || c == '-';
}

// Reader functions

#define SEXPR_ERROR (-1024)
#define SEXPR_EOF (-1025)

static int reader_read_char(reader_state_t *rs)
{
    if(rs->eof) return SEXPR_EOF;

    lua_rawgeti(rs->L, LUA_REGISTRYINDEX, rs->reader_ref);
    lua_pushstring(rs->L, "read");
    lua_gettable(rs->L, -2);
    lua_pushvalue(rs->L, -2); // self
    lua_pushinteger(rs->L, 1);  // Read exactly 1 character

    if(lua_pcall(rs->L, 2, 1, 0) != LUA_OK)
    {
        return SEXPR_ERROR;
    }

    if(lua_isnil(rs->L, -1))
    {
        rs->eof = 1;
        lua_pop(rs->L, 2);
        return SEXPR_EOF;
    }

    size_t len;
    const char *data = lua_tolstring(rs->L, -1, &len);
    if(len == 0)
    {
        rs->eof = 1;
        lua_pop(rs->L, 2);
        return SEXPR_EOF;
    }

    rs->current_char = (unsigned char) data[0];
    rs->has_char = 1;

    lua_pop(rs->L, 2);
    return 1;
}

static int reader_peek(reader_state_t *rs)
{
    if(!rs->has_char)
    {
        int result = reader_read_char(rs);
        if(result <= SEXPR_ERROR) return result;
    }
    return rs->current_char;
}

static int reader_next(reader_state_t *rs)
{
    int c = reader_peek(rs);
    if(c > SEXPR_ERROR) rs->has_char = 0;  // Mark that we need to read a new character
    return c;
}

static void skip_whitespace(reader_state_t *rs)
{
    int c;
    while((c = reader_peek(rs)) > SEXPR_ERROR && is_whitespace(c))
    {
        reader_next(rs);
    }
}

// Symbol functions
static int create_symbol(lua_State *L, const char *name, size_t len)
{
    sexpr_symbol_t *sym = lua_newuserdata(L, sizeof(sexpr_symbol_t) + len + 1);
    sym->len = len;
    memcpy(sym->name, name, len);
    sym->name[len] = '\0';

    luaL_getmetatable(L, SEXPR_SYMBOL_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static int symbol_tostring(lua_State *L)
{
    sexpr_symbol_t *sym = luaL_checkudata(L, 1, SEXPR_SYMBOL_METATABLE);
    lua_pushstring(L, sym->name);
    return 1;
}

static int symbol_eq(lua_State *L)
{
    sexpr_symbol_t *sym1 = luaL_checkudata(L, 1, SEXPR_SYMBOL_METATABLE);
    sexpr_symbol_t *sym2 = luaL_checkudata(L, 2, SEXPR_SYMBOL_METATABLE);

    lua_pushboolean(L, sym1->len == sym2->len &&
                    memcmp(sym1->name, sym2->name, sym1->len) == 0);
    return 1;
}

// Parsing functions
static int parse_string(reader_state_t *rs)
{
    luaL_Buffer buf;
    luaL_buffinit(rs->L, &buf);

    // Skip opening quote
    reader_next(rs);

    int c;
    while((c = reader_next(rs)) > SEXPR_ERROR)
    {
        if(c == '"')
        {
            luaL_pushresult(&buf);
            return 1;
        }
        else if(c == '\\')
        {
            c = reader_next(rs);
            if(c <= SEXPR_ERROR) break;

            switch(c)
            {
            case '\\': luaL_addchar(&buf, '\\');
                break;
            case 'n': luaL_addchar(&buf, '\n');
                break;
            case 'r': luaL_addchar(&buf, '\r');
                break;
            case '"': luaL_addchar(&buf, '"');
                break;
            case 't': luaL_addchar(&buf, '\t');
                break;
            default:
                luaL_addchar(&buf, '\\');
                luaL_addchar(&buf, c);
                break;
            }
        }
        else
        {
            luaL_addchar(&buf, c);
        }
    }

    lua_pushnil(rs->L);
    lua_pushstring(rs->L, "unterminated string");
    return 2;
}

static int parse_number(reader_state_t *rs)
{
    luaL_Buffer buf;
    luaL_buffinit(rs->L, &buf);

    int c = reader_peek(rs);
    // FIXME: handle c <= ERROR
    if(c == '-')
    {
        luaL_addchar(&buf, reader_next(rs));
        c = reader_peek(rs);
    }

    // Parse integer part
    if(!isdigit(c))
    {
        lua_pushnil(rs->L);
        lua_pushstring(rs->L, "invalid number");
        return 2;
    }

    while((c = reader_peek(rs)) > SEXPR_ERROR && isdigit(c))
    {
        luaL_addchar(&buf, reader_next(rs));
    }

    // Parse decimal part if present
    if(c == '.')
    {
        luaL_addchar(&buf, reader_next(rs));

        c = reader_peek(rs);
        // FIXME: handle c <= ERROR
        if(!isdigit(c))
        {
            lua_pushnil(rs->L);
            lua_pushstring(rs->L, "invalid number");
            return 2;
        }

        while((c = reader_peek(rs)) > SEXPR_ERROR && isdigit(c))
        {
            luaL_addchar(&buf, reader_next(rs));
        }
    }

    luaL_pushresult(&buf);

    // Convert to number
    lua_Number num = lua_tonumber(rs->L, -1);
    lua_pop(rs->L, 1);
    lua_pushnumber(rs->L, num);

    return 1;
}

static int parse_symbol_or_bool(reader_state_t *rs)
{
    luaL_Buffer buf;
    luaL_buffinit(rs->L, &buf);

    int c;
    while((c = reader_peek(rs)) > SEXPR_ERROR && is_symbol_char(c))
    {
        luaL_addchar(&buf, reader_next(rs));
    }

    luaL_pushresult(&buf);

    size_t len;
    const char *str = lua_tolstring(rs->L, -1, &len);

    // Check for boolean
    if(len == 4 && memcmp(str, "true", 4) == 0)
    {
        lua_pop(rs->L, 1);
        lua_pushboolean(rs->L, 1);
        return 1;
    }
    else if(len == 5 && memcmp(str, "false", 5) == 0)
    {
        lua_pop(rs->L, 1);
        lua_pushboolean(rs->L, 0);
        return 1;
    }

    // It's a symbol
    lua_pop(rs->L, 1);
    return create_symbol(rs->L, str, len);
}

static int parse_list(reader_state_t *rs)
{
    lua_newtable(rs->L);

    // Skip opening paren
    reader_next(rs);

    int index = 1;
    while(1)
    {
        skip_whitespace(rs);

        int c = reader_peek(rs);
        if(c <= SEXPR_ERROR)
        {
            lua_pushnil(rs->L);
            lua_pushstring(rs->L, "unterminated list");
            return 2;
        }

        if(c == ')')
        {
            // FIXME: handle ERROR case
            reader_next(rs);
            return 1;
        }

        int result = read_datum(rs);
        if(result != 1)
        {
            return result;
        }

        lua_rawseti(rs->L, -2, index++);
    }
}

static int read_datum(reader_state_t *rs)
{
    skip_whitespace(rs);

    int c = reader_peek(rs);
    if(c <= SEXPR_ERROR)
    {
        lua_pushnil(rs->L);
        lua_pushstring(rs->L, "unexpected end of input");
        return 2;
    }

    switch(c)
    {
    case '(':
        return parse_list(rs);
    case '"':
        return parse_string(rs);
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return parse_number(rs);
    default:
        if(is_symbol_char(c))
        {
            return parse_symbol_or_bool(rs);
        }
        lua_pushnil(rs->L);
        lua_pushfstring(rs->L, "unexpected character: %c", c);
        return 2;
    }
}

// Writing functions
static int write_string_escaped(lua_State *L, int writer_ref, const char *str, size_t len)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
    lua_pushstring(L, "write");
    lua_gettable(L, -2);
    lua_pushvalue(L, -2); // self
    lua_pushstring(L, "\"");

    if(lua_pcall(L, 2, 0, 0) != LUA_OK)
    {
        return 0;
    }

    for(size_t i = 0; i < len; i++)
    {
        char c = str[i];
        const char *escaped = NULL;

        switch(c)
        {
        case '\\': escaped = "\\\\";
            break;
        case '\n': escaped = "\\n";
            break;
        case '\r': escaped = "\\r";
            break;
        case '"': escaped = "\\\"";
            break;
        case '\t': escaped = "\\t";
            break;
        default:
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
                lua_pushstring(L, "write");
                lua_gettable(L, -2);
                lua_pushvalue(L, -2); // self
                lua_pushlstring(L, &c, 1);

                if(lua_pcall(L, 2, 0, 0) != LUA_OK)
                {
                    return 0;
                }
                lua_pop(L, 1);
                continue;
            }
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
        lua_pushstring(L, "write");
        lua_gettable(L, -2);
        lua_pushvalue(L, -2); // self
        lua_pushstring(L, escaped);

        if(lua_pcall(L, 2, 0, 0) != LUA_OK)
        {
            return 0;
        }
        lua_pop(L, 1);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
    lua_pushstring(L, "write");
    lua_gettable(L, -2);
    lua_pushvalue(L, -2); // self
    lua_pushstring(L, "\"");

    if(lua_pcall(L, 2, 0, 0) != LUA_OK)
    {
        return 0;
    }
    lua_pop(L, 1);

    return 1;
}

static int write_datum(lua_State *L, int writer_ref, int datum_idx)
{
    int type = lua_type(L, datum_idx);

    switch(type)
    {
    case LUA_TBOOLEAN:
        {
            const char *str = lua_toboolean(L, datum_idx) ? "true" : "false";

            lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
            lua_pushstring(L, "write");
            lua_gettable(L, -2);
            lua_pushvalue(L, -2); // self
            lua_pushstring(L, str);

            if(lua_pcall(L, 2, 0, 0) != LUA_OK)
            {
                return 0;
            }
            lua_pop(L, 1);
            return 1;
        }

    case LUA_TNUMBER:
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
            lua_pushstring(L, "write");
            lua_gettable(L, -2);
            lua_pushvalue(L, -2); // self
            lua_pushvalue(L, datum_idx);
            lua_tostring(L, -1);

            if(lua_pcall(L, 2, 0, 0) != LUA_OK)
            {
                return 0;
            }
            lua_pop(L, 1);
            return 1;
        }

    case LUA_TSTRING:
        {
            size_t len;
            const char *str = lua_tolstring(L, datum_idx, &len);
            return write_string_escaped(L, writer_ref, str, len);
        }

    case LUA_TTABLE:
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
            lua_pushstring(L, "write");
            lua_gettable(L, -2);
            lua_pushvalue(L, -2); // self
            lua_pushstring(L, "(");

            if(lua_pcall(L, 2, 0, 0) != LUA_OK)
            {
                return 0;
            }

            lua_pop(L, 1);

            size_t len = lua_rawlen(L, datum_idx);
            for(size_t i = 1; i <= len; i++)
            {
                if(i > 1)
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
                    lua_pushstring(L, "write");
                    lua_gettable(L, -2);
                    lua_pushvalue(L, -2); // self
                    lua_pushstring(L, " ");

                    if(lua_pcall(L, 2, 0, 0) != LUA_OK)
                    {
                        return 0;
                    }
                    lua_pop(L, 1);
                }

                lua_rawgeti(L, datum_idx, i);
                if(!write_datum(L, writer_ref, lua_gettop(L)))
                {
                    return 0;
                }
                lua_pop(L, 1);
            }

            lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
            lua_pushstring(L, "write");
            lua_gettable(L, -2);
            lua_pushvalue(L, -2); // self
            lua_pushstring(L, ")");

            if(lua_pcall(L, 2, 0, 0) != LUA_OK)
            {
                return 0;
            }
            lua_pop(L, 1);
            return 1;
        }

    case LUA_TUSERDATA:
        {
            if(!lua_getmetatable(L, datum_idx))
            {
                return 0;
            }
            luaL_getmetatable(L, SEXPR_SYMBOL_METATABLE);
            if(lua_rawequal(L, -1, -2))
            {
                sexpr_symbol_t *sym = lua_touserdata(L, datum_idx);

                lua_rawgeti(L, LUA_REGISTRYINDEX, writer_ref);
                lua_pushstring(L, "write");
                lua_gettable(L, -2);
                lua_pushvalue(L, -2); // self
                lua_pushlstring(L, sym->name, sym->len);

                if(lua_pcall(L, 2, 0, 0) != LUA_OK)
                {
                    return 0;
                }
                lua_pop(L, 3);
                return 1;
            }
            return 0;
        }

    default:
        return 0;
    }
}

// Lua API functions
static int sexpr_read(lua_State *L)
{
    luaL_checkany(L, 1);

    reader_state_t rs;
    rs.L = L;
    rs.reader_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    rs.has_char = 0;
    rs.eof = 0;

    skip_whitespace(&rs);
    int c = reader_peek(&rs);
    if(c <= SEXPR_ERROR)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, rs.reader_ref);
        lua_pushnil(L);
        lua_pushstring(L, "EOF");
        return 2;
    }

    int result = read_datum(&rs);

    luaL_unref(L, LUA_REGISTRYINDEX, rs.reader_ref);

    return result;
}

static int sexpr_write(lua_State *L)
{
    luaL_checkany(L, 1);
    luaL_checkany(L, 2);

    lua_pushvalue(L, 1);
    int writer_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    int success = write_datum(L, writer_ref, 2);

    luaL_unref(L, LUA_REGISTRYINDEX, writer_ref);

    if(success)
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    else
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "write error");
        return 2;
    }
}

static int sexpr_is_symbol(lua_State *L)
{
    lua_pushboolean(L, luaL_testudata(L, 1, SEXPR_SYMBOL_METATABLE) != NULL);
    return 1;
}

static int sexpr_symbol_name(lua_State *L)
{
    sexpr_symbol_t *sym = luaL_checkudata(L, 1, SEXPR_SYMBOL_METATABLE);
    lua_pushlstring(L, sym->name, sym->len);
    return 1;
}

static int sexpr_new_symbol(lua_State *L)
{
    size_t len = 0;
    const char *name = luaL_checklstring(L, 1, &len);
    create_symbol(L, name, len);
    return 1;
}

// Module registration
static const luaL_Reg sexpr_funcs[] = {
    {"read", sexpr_read},
    {"write", sexpr_write},
    {"is_symbol", sexpr_is_symbol},
    {"symbol_name", sexpr_symbol_name},
    {"new_symbol", sexpr_new_symbol},
    {NULL, NULL}
};

static const luaL_Reg symbol_metamethods[] = {
    {"__tostring", symbol_tostring},
    {"__eq", symbol_eq},
    {NULL, NULL}
};

LUALIB_API int luaopen_sexpr(lua_State *L)
{
    // Create symbol metatable
    luaL_newmetatable(L, SEXPR_SYMBOL_METATABLE);
    luaL_setfuncs(L, symbol_metamethods, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // Create module table
    luaL_newlib(L, sexpr_funcs);

    return 1;
}
