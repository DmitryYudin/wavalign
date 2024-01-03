#!/bin/bash

set -eu

cd "$(dirname "$0")" >/dev/null 2>&1

clang-format() { command clang-format -i ${LINT_STRICT:+--Werror --dry-run} --style=file:.clang-format "$@"; }

if [ $# == 0 ]; then
    for d in src test; do
        clang-format $(find $d -iname '*.h' -o -iname '*.c*')
    done
else
    clang-format "$@"
fi
