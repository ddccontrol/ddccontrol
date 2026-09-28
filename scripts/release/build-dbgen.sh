#!/usr/bin/env bash
# Native musl builds keep release executables independent of the host glibc.
set -euo pipefail
export LC_ALL=C
target=${1:?usage: build-dbgen.sh TARGET VERSION OUTPUT_DIRECTORY}
version=${2:?expected release version}
output=$(realpath -m "${3:?expected output directory}")
toolchain=${DBGEN_RUST_TOOLCHAIN:-1.85.0}
case "$(uname -m):$target" in
    x86_64:x86_64-unknown-linux-musl | aarch64:aarch64-unknown-linux-musl) ;;
    *) echo "Use a native runner for $target" >&2; exit 1 ;;
esac
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Expected a stable version' >&2; exit 1; }
source_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$source_root"
source_version=$(sed -n 's/^AC_INIT(\[DDC\/CI control tool\],\[\([^]]*\)\].*/\1/p' configure.ac)
[[ "$version" == "$source_version" ]] || { echo 'Release version differs from source' >&2; exit 1; }
export CARGO_TARGET_DIR="$output/target"
export CARGO_ENCODED_RUSTFLAGS=$'-Ctarget-feature=+crt-static\x1f-Clinker=musl-gcc\x1f'"--remap-path-prefix=$source_root=."
cargo +"$toolchain" test --locked --release --target "$target" -p ddccontrol-dbgen
cargo +"$toolchain" build --locked --release --target "$target" -p ddccontrol-dbgen
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
install -m 755 "$CARGO_TARGET_DIR/$target/release/ddccontrol-dbgen" "$stage/ddccontrol-dbgen"
strip --strip-unneeded "$stage/ddccontrol-dbgen"
./scripts/release/check-dbgen.sh "$stage/ddccontrol-dbgen"
install -m 644 COPYING "$stage/COPYING"
jq -n --arg commit "$(git rev-parse HEAD)" --arg version "$version" --arg target "$target" \
    --arg rust "$(rustc +"$toolchain" --version)" \
    '{commit: $commit, version: $version, target: $target, rust: $rust}' > "$stage/provenance.json"
chmod 644 "$stage/provenance.json"
mkdir -p "$output/artifacts"
archive="ddccontrol-dbgen-$version-$target.tar.gz"
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$stage" -cf - \
    COPYING ddccontrol-dbgen provenance.json | gzip -n > "$output/artifacts/$archive"
(cd "$output/artifacts" && sha256sum "$archive" > "$archive.sha256" && sha256sum -c "$archive.sha256")
# Extraction and execution also check archive paths, file modes and packaging.
mkdir "$stage/unpacked"
tar -C "$stage/unpacked" -xzf "$output/artifacts/$archive"
./scripts/release/check-dbgen.sh "$stage/unpacked/ddccontrol-dbgen"
