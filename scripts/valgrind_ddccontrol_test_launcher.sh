#!/usr/bin/env bash

source "$(dirname "$0")/common_launcher.sh"

set -e

# install and launch test
sudo make --silent install
sudo ./scripts/valgrind_ddccontrol_test.sh
