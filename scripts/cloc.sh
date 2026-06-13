#!/usr/bin/env bash

srcdirs=(pdc bepd runtime)

pd_cloc="$(lr -e '\.pd$' "${srcdirs[@]}" | xe -N0 -- cat | grep -c '[^[:space:]]')"
c_cloc="$(lr -e '\.[ch]$' "${srcdirs[@]}" | xe -N0 -- cat | grep -c '[^[:space:]]')"
pd_wcloc="$(lr -e '\.pd$' "${srcdirs[@]}" | xe -N0 -- cat | wc -l)"
c_wcloc="$(lr -e '\.[ch]$' "${srcdirs[@]}" | xe -N0 -- cat | wc -l)"

echo 'Líneas (sin líneas vacías):'
printf "%-12s %s\n" "pseudod" "$pd_cloc"
printf "%-12s %s\n" "c" "$c_cloc"
printf "%-12s %s\n" "total" "$((c_cloc + pd_cloc))"

echo
echo 'Líneas totales:'
printf "%-12s %s\n" "pseudod" "$pd_wcloc"
printf "%-12s %s\n" "c" "$c_wcloc"
printf "%-12s %s\n" "total" "$((c_wcloc + pd_wcloc))"
