# Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)
# SPDX-License-Identifier: GPL-2.0

SUPPRESSIONS=( $(pwd)/scripts/tmp/GNOME.supp/build/{base,gio,glib,gtk,gtk3}.supp /usr/share/glib-2.0/valgrind/glib.supp )

for f in "${SUPPRESSIONS[@]}";
do 
    [ ! -f "$f" ] && echo "Suppresion file is missing: ${f}" && exit 1;
done

VALGRIND_SUPPRESSION_PARAMS="${SUPPRESSIONS[*]/#/--suppressions=}"
