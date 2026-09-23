#! /bin/sh

# Merges static library ar archives together. Stolen from  libretro/TIC-80.
# Usage: merge_static.sh <ar_tool> out.a in1.a in2.a  in3.a ...

AR="$1"
shift
OUTPUT="$1"
shift

(echo create "$OUTPUT";
    for x in "$@"; do
        echo addlib "$x"
    done
    echo save
    echo end
) | "$AR" -M
