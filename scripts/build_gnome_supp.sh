#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ ! -d ${DIR}/tmp/GNOME.supp ]; then
    mkdir -p ${DIR}/tmp;
    (cd ${DIR}/tmp; git clone https://github.com/dtrebbien/GNOME.supp.git);
fi

(cd ${DIR}/tmp/GNOME.supp; make)
