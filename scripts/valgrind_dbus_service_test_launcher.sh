#!/usr/bin/env bash

# Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)
# SPDX-License-Identifier: GPL-2.0

source "$(dirname "$0")/common_launcher.sh"

# install
echo 'make install; remove dbus service'
sudo make --silent install
sudo find /usr/{,local/}share/dbus* -name "ddccontrol.DDCControl.service" -exec rm '{}' \;

# launch test
sudo ./scripts/valgrind_dbus_service_test.sh
