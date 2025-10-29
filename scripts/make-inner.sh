#!/usr/bin/env bash

die() {
  printf "$@" >&2
  exit 1
}

SOURCE_DIR=""
BINARY_DIR=""
OUTPUT_FILE=""

while [[ "$#" -gt 0 ]]; do
  flag="$1"
  shift

  case "$flag" in
  --source-dir)
    SOURCE_DIR="$1"
    shift
    ;;
  --binary-dir)
    BINARY_DIR="$1"
    shift
    ;;
  --output)
    OUTPUT_FILE="$1"
    shift
    ;;
  *)
    die "Unexpected argument: %s\n" "$flag"
  esac
done

if [[ -z "$SOURCE_DIR" ]]; then
  die "Must set --source-dir"
fi
if [[ -z "$BINARY_DIR" ]]; then
  die "Must set --binary-dir"
fi
if [[ -z "$OUTPUT_FILE" ]]; then
  die "Must set --output"
fi

cd "$SOURCE_DIR" || die "Could not cd"

built_exe=.pseudod-build/compilado/pdc/inicio.exe

rm "$built_exe"
lua scripts/build/build.lua compilar pdc
cp "$built_exe" "$OUTPUT_FILE"
