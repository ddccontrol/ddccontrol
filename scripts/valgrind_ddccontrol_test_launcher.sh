#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( dirname "${DIR}" )"

cd "${PROJECT_DIR}"

# prepare
make --silent -j4
./scripts/build_gnome_supp.sh

# install and launch test
sudo make --silent install
sudo ./scripts/valgrind_ddccontrol_test.sh
