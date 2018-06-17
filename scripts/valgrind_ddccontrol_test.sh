#!/usr/bin/env bash

# Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)
# SPDX-License-Identifier: GPL-2.0

source "$(dirname "$0")/common_test.sh"

DEVICE=$1

DDCCONTROL_NO_DAEMON=1
export DDCCONTROL_NO_DAEMON

if [ -z "${DEVICE}" ]
then
    DEVICE=$(ddccontrol -p | grep '^ - Device: ' | head -n1 | tr ' ' '\n' | tail -n1)
    echo "Selected device: ${DEVICE}, launch with device argument for faster startup"
else
    echo "Using device: ${DEVICE}"
fi

function valgrind_ddccontrol () {
    VALGRIND_OUT=$(mktemp /tmp/ddccontrol.valgrind.out.XXXXXXXX)

    export G_SLICE=always-malloc
    export G_DEBUG=gc-friendly

    valgrind \
        --leak-check=full \
        --error-exitcode=121 \
        --log-file="${VALGRIND_OUT}" \
        ${SUPPRESSIONS[@]/#/--suppressions=} \
        ddccontrol $@

    EXIT_CODE=$?

    cat "${VALGRIND_OUT}"
    rm "${VALGRIND_OUT}"

    if [ ${EXIT_CODE} -ne 0 ]
    then
        echo "Execution returned ${EXIT_CODE}, arguments: $*"
        exit 1
    fi
}

valgrind_ddccontrol -r 0x10 ${DEVICE}
valgrind_ddccontrol -r 0x10 -w 21 ${DEVICE}
