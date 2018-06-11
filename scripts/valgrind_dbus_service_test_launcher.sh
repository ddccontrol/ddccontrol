#!/usr/bin/env bash

source "$(dirname "$0")/common_launcher.sh"

# install
echo 'make install; remove dbus service'
sudo make --silent install
sudo find /usr/{,local/}share/dbus* -name "ddccontrol.DDCControl.service" -exec rm '{}' \;

# launch test
sudo ./scripts/valgrind_dbus_service_test.sh
