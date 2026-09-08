#!/usr/bin/env bash
# Build in a disposable Debian container, using Debian's packaged Rust crates.
set -euo pipefail

archive=$(realpath "$1")
packaging=$(realpath "$2")
version=$3
output=$(realpath -m "$4")
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential ca-certificates devscripts equivs patch python3

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
tar -xf "$archive" -C "$work"
cd "$work/ddccontrol-$version"
cp -a "$packaging/debian" .
patch --fuzz=0 -p1 < "$script_dir/debian.patch"
mv debian/libddccontrol0.install debian/libddccontrol1.install
mv debian/libddccontrol0.maintscript debian/libddccontrol1.maintscript
# dh_makeshlibs derives the current SONAME and package name from the built library.
rm debian/libddccontrol0.shlibs
export DEBFULLNAME='DDCcontrol release automation'
export DEBEMAIL='ddccontrol@users.noreply.github.com'
dch --newversion "$version-1~trixie1" --distribution trixie \
    --force-distribution 'Build the upstream release for the DDCcontrol repository.'
mk-build-deps --install --remove \
    --tool 'apt-get -y --no-install-recommends' debian/control

# The upstream lockfile selects crates.io versions. Resolve the same declared
# dependency ranges against Debian's registry, without network access or vendoring.
export CARGO_HOME="$work/cargo"
mkdir -p "$CARGO_HOME"
cat > "$CARGO_HOME/config.toml" <<'EOF'
[source.crates-io]
replace-with = "debian"
[source.debian]
directory = "/usr/share/cargo/registry"
[net]
offline = true
EOF
rm Cargo.lock
cargo generate-lockfile --offline
dpkg-buildpackage --build=binary --no-sign -j"$(nproc)"

arch=$(dpkg --print-architecture)
destination="$output/debian/trixie/$arch"
mkdir -p "$destination"
cp "$work"/*.deb "$work"/*.buildinfo "$work"/*.changes "$destination/"
# Fail if dh_shlibdeps did not resolve the library to the produced package.
dpkg-deb -f "$destination"/ddccontrol_*.deb Depends | grep 'libddccontrol1' > /dev/null
dpkg-deb -f "$destination"/gddccontrol_*.deb Depends | grep 'libddccontrol1' > /dev/null
test -f "$destination/libddccontrol1_${version}-1~trixie1_${arch}.deb"
