#!/usr/bin/env bash
# Compare translated XML/CBOR trees using the synthetic-CAPS C test driver.
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "Usage: $0 DRIVER DATABASE_SOURCE_DIRECTORY [UTF8_LOCALE]" >&2
    exit 2
fi
gettext_driver=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
gettext_source=$(cd "$2" && pwd)
gettext_locale=${3:-en_US.UTF-8}
if [[ ! -f "$gettext_source/options.xml" && -f "$gettext_source/db/options.xml" ]]; then
    gettext_source="$gettext_source/db"
fi
gettext_catalog="$gettext_source/../po/fr.po"
for gettext_file in "$gettext_driver" "$gettext_source/options.xml" \
    "$gettext_source/ddccontrol-db.cbor" "$gettext_catalog"; do
    if [[ ! -f "$gettext_file" ]]; then
        echo "Missing test input: $gettext_file" >&2
        exit 2
    fi
done
if [[ ! -x "$gettext_driver" || ! -d "$gettext_source/monitor" ]]; then
    echo 'The driver must be executable and the database must contain monitor/.' >&2
    exit 2
fi
command -v msgfmt >/dev/null
if [[ $(LC_ALL="$gettext_locale" locale charmap 2>/dev/null) != UTF-8 ]]; then
    echo "Install a UTF-8 locale or select one explicitly; unavailable: $gettext_locale" >&2
    exit 2
fi

gettext_work=$(mktemp -d)
trap 'rm -rf "$gettext_work"' EXIT
mkdir -p "$gettext_work/xml" "$gettext_work/cbor" "$gettext_work/locale/fr/LC_MESSAGES"
cp "$gettext_source/options.xml" "$gettext_work/xml/options.xml"
cp -R "$gettext_source/monitor" "$gettext_work/xml/monitor"
if [[ -f "$gettext_source/ddccontrol-db.snapshot" ]]; then
    cp "$gettext_source/ddccontrol-db.snapshot" "$gettext_work/xml/ddccontrol-db.snapshot"
fi
cp "$gettext_source/ddccontrol-db.cbor" "$gettext_work/cbor/ddccontrol-db.cbor"
msgfmt -o "$gettext_work/locale/fr/LC_MESSAGES/ddccontrol-db.mo" "$gettext_catalog"

export DDCCONTROL_TEST_LOCALEDIR="$gettext_work/locale"
LC_ALL="$gettext_locale" LANGUAGE=fr "$gettext_driver" "$gettext_work/xml" VESA > "$gettext_work/xml-fr.tree"
LC_ALL="$gettext_locale" LANGUAGE=fr "$gettext_driver" "$gettext_work/cbor" VESA > "$gettext_work/cbor-fr.tree"
diff -u "$gettext_work/xml-fr.tree" "$gettext_work/cbor-fr.tree"
LC_ALL=C LANGUAGE=C "$gettext_driver" "$gettext_work/cbor" VESA > "$gettext_work/cbor-C.tree"
if cmp -s "$gettext_work/cbor-fr.tree" "$gettext_work/cbor-C.tree"; then
    echo "No French translation observed; choose a non-C UTF-8 locale (used $gettext_locale)." >&2
    exit 1
fi
echo "Gettext parity passed: XML and CBOR have identical French trees under $gettext_locale; C-locale output differs."
