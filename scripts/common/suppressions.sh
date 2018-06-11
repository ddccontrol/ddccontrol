
SUPPRESSIONS=( $(pwd)/scripts/tmp/GNOME.supp/build/{base,gio,glib,gtk,gtk3}.supp /usr/share/glib-2.0/valgrind/glib.supp )

for f in "${SUPPRESSIONS[@]}";
do 
    [ ! -f "$f" ] && echo "Suppresion file is missing: ${f}" && exit 1;
done

VALGRIND_SUPPRESSIONS="${SUPPRESSIONS[@]/#/--suppressions=}"
VALGRIND_SUPPRESSION_PARAMS=${VALGRIND_SUPPRESSIONS[*]}
