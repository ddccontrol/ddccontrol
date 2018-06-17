#!/usr/bin/env bash

# Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)
# SPDX-License-Identifier: GPL-2.0

source "$(dirname "$0")/common_test.sh"

VALGRIND_OUT=$(mktemp /tmp/ddccontrol_service.valgrind.out.XXXXXXXX)
chmod 755 "${VALGRIND_OUT}"

echo "kill all ddccontrol & ddcpci processes"
pkill ddccontrol
pkill ddcpci

sleep 0.25

pgrep ddc

echo "starting service"
su -c "libtool --mode=execute valgrind --leak-check=full ${VALGRIND_SUPPRESSION_PARAMS} --log-file='${VALGRIND_OUT}' '$(pwd)/src/daemon/ddccontrol_service'" &

DDCCONTROL_DBUS_SERVICE_PID=$!

sleep 5

echo "run tests"
"$(pwd)/src/ddccontrol/ddccontrol" -p >/dev/null

kill ${DDCCONTROL_DBUS_SERVICE_PID}

echo "valgrind output written to: ${VALGRIND_OUT}"
