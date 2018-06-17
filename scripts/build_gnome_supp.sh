#!/usr/bin/env bash

# Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)
# SPDX-License-Identifier: GPL-2.0

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ ! -d "${DIR}/tmp/GNOME.supp" ]; then
    mkdir -p "${DIR}/tmp"
    (cd "${DIR}/tmp"; git clone 'https://github.com/dtrebbien/GNOME.supp.git');
fi

(cd "${DIR}/tmp/GNOME.supp"; make)
