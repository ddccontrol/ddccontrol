#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
frozen="$root/crates/ddccontrol-db/tests/frozen-v1"
fixtures="$root/crates/ddccontrol-db/fixtures/cbor"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
(cd "$frozen" && sha256sum --check SHA256SUMS)
target=${CARGO_TARGET_DIR:-$frozen/target}
# Pin the build output to the path linked below, including when Cargo has a
# different build.target-dir configured. Never link a stale default archive.
cargo build --manifest-path "$frozen/Cargo.toml" --release --locked --features gettext --target-dir "$target"
"${CC:-cc}" -Wall -Wextra -Werror "$frozen/driver.c" \
    "$target/release/libddccontrol_db_frozen_v1.a" -ldl -lpthread -lm -o "$work/reader"
export LC_ALL=C LANGUAGE=C
cp "$fixtures/reference-v1.cbor" "$work/ddccontrol-db.cbor"
"$work/reader" "$work" TST0001 > "$work/base.tree"
diff -u "$fixtures/reference-v1.tree" "$work/base.tree"
cp "$fixtures/newer-v1.cbor" "$work/ddccontrol-db.cbor"
"$work/reader" "$work" TST0001 > "$work/unchanged.tree"
diff -u "$work/base.tree" "$work/unchanged.tree"
"$work/reader" "$work" NEW0001 > "$work/new.tree"
diff -u "$fixtures/newer-v1.tree" "$work/new.tree"
"$work/reader" "$work" VESA > "$work/generic.tree"
cp "$fixtures/descriptions-v1.cbor" "$work/ddccontrol-db.cbor"
if "$work/reader" "$work" TST0001 > "$work/rejected.tree"; then
    echo 'required profile feature was silently accepted' >&2
    exit 1
else
    test "$?" = 3
fi
test ! -s "$work/rejected.tree"
"$work/reader" "$work" VESA > "$work/unaffected.tree"
diff -u "$work/generic.tree" "$work/unaffected.tree"
echo 'Frozen first-v1 reader: unchanged profiles, new profiles/codes/values, optional fields, required isolation passed.'
