#!/usr/bin/sh

find() {
    which "$1" >/dev/null
}

eecho() {
    echo "$@" 1>&2
}

checkprogram() {
    find "$1" || {
        eecho "$2"
        notfound=1
    }
}

notfound=0

checkprogram ysh "The YSH shell is not installed, see https://www.oilshell.org/"
checkprogram lr "The lr program is not installed, see https://git.vuxu.org/lr/about/"
checkprogram xe "The xe program is not installed, see https://git.vuxu.org/xe/about/"
checkprogram awk "The AWK language is not installed"
checkprogram python3 "The Python 3 language is not installed"

if [ "$notfound" = "1" ]; then
    exit 1
fi
