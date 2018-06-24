#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( dirname "$DIR" )"

# for now, only added folders heavily touched in this release
PATHS=(
    "${PROJECT_DIR}/src/ddccontrol/"
    "${PROJECT_DIR}/src/daemon/"
)

astyle --recursive \
    --style=linux --indent=tab --align-pointer=name \
    --unpad-paren --pad-header --pad-oper \
    "${PATHS[@]/%/\*.h}" "${PATHS[@]/%/\*.c}"
