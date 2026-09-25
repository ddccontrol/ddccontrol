#!/usr/bin/env bash
# Run the exact standalone executable that is going into the release archive.
set -euo pipefail
export LC_ALL=C
binary=$(realpath "${1:?usage: check-dbgen.sh BINARY}")
source_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

readelf --file-header "$binary" > "$work/header"
readelf --program-headers "$binary" > "$work/program-headers"
readelf --dynamic "$binary" > "$work/dynamic"
if grep -q INTERP "$work/program-headers" || grep -q NEEDED "$work/dynamic"; then
    echo "Producer must be statically linked, with no ELF interpreter or shared libraries" >&2
    exit 1
fi
"$binary" --help
"$binary" --version
"$binary" convert "$source_root/crates/ddccontrol-dbgen/fixtures/source" "$work/base.cbor" \
    --revision fixture-1 --snapshot "$work/base.snapshot"
cmp "$work/base.cbor" "$source_root/crates/ddccontrol-db/fixtures/cbor/reference-v1.cbor"
cmp "$work/base.snapshot" "$source_root/crates/ddccontrol-dbgen/fixtures/base-v1.snapshot"
"$binary" validate "$work/base.cbor"
"$binary" dump "$work/base.cbor" > "$work/base.json"
"$binary" rewrite "$work/base.cbor" "$work/rewritten.cbor"
cmp "$work/base.cbor" "$work/rewritten.cbor"
printf '\xff' > "$work/invalid.cbor"
if "$binary" validate "$work/invalid.cbor"; then
    echo "Producer accepted an invalid database" >&2
    exit 1
fi
