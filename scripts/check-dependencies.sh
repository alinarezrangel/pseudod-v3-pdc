#!/usr/bin/sh

find() {
    which "$1" >/dev/null
}

eecho() {
    echo "$@" 1>&2
}

checkprogram() {
    # checkprogram exe err
    printf "check %s " "$1"
    find "$1" && printf "ok\n" || {
        printf "\n"
        eecho "$2"
        notfound=1
    }
}

notfound=0

checkprogram ysh "El shell YSH no está instalado, véase https://www.oilshell.org/"
checkprogram lr "El programa lr no está instalado, véase https://git.vuxu.org/lr/about/"
checkprogram xe "El programa xe no está instalado, véase https://git.vuxu.org/xe/about/"
checkprogram awk "El programa awk no está instalado"
checkprogram lua5.4 "Lua 5.4 no está instalado, véase https://www.lua.org/"

if [ "$notfound" = "1" ]; then
    exit 1
fi

notfound=0

checkluadep() {
    # checkluadep require err
    printf "check %s " "$1"
    lua5.4 -l"$1" -e 'print"ok"' 2>/dev/null || {
        printf "\n"
        eecho "$2"
        notfound=1
    }
}

checkluadep lpeg "LPEG debe estar instalado, véase https://www.inf.puc-rio.br/~roberto/lpeg/"
checkluadep re "LPEG debe estar instalado, véase https://www.inf.puc-rio.br/~roberto/lpeg/"
checkluadep lsqlite3 "LuaSqlite3 debe estar instalado, véase http://lua.sqlite.org/index.cgi/doc/tip/doc/lsqlite3.wiki"
checkluadep posix "Luaposix debe estar instalado, véase https://github.com/luaposix/luaposix/"

if [ "$notfound" = "1" ]; then
    exit 1
fi
