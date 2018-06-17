#!/usr/bin/env bash

# Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)
# SPDX-License-Identifier: GPL-2.0

source "$(dirname "$0")/common_launcher.sh"

set -e

# install and launch test
sudo make --silent install
sudo ./scripts/valgrind_ddccontrol_test.sh
