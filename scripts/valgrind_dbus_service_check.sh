#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( dirname "${DIR}" )"

make --silent -j4

[ `whoami` = root ] || exec sudo su -c $0 root

source "${DIR}/common/suppressions.sh"

echo 'make install; remove dbus service'
make --silent install
find /usr/{,local/}share/dbus* -name "ddccontrol.DDCControl.service" -exec rm '{}' \;

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
