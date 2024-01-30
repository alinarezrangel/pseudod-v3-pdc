#!/usr/bin/env ysh

lr -t 'name ~~ "*.pd" && path ~~ "pdc/*"' | \
    xe wc -l | \
    sort -n | \
    awk 'BEGIN {x = 0} {print $0; x += $1} END{ print x }'
