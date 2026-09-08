#!/usr/bin/env bash
# Build in a disposable Fedora container using the pinned Fedora spec.
set -euo pipefail

archive=$(realpath "$1")
packaging=$(realpath "$2")
version=$3
output=$(realpath -m "$4")
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]

dnf install -y rpm-build dnf5-plugins
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work"/{SOURCES,SPECS}
cp "$packaging/ddccontrol.spec" "$work/SPECS/"
cp "$archive" "$work/SOURCES/ddccontrol-$version-vendor.tar.gz"
sed -i -E \
    -e "s/^(Version:)[[:space:]]+.*/\1          $version/" \
    -e 's/^(Release:)[[:space:]]+.*/\1          1%{?dist}/' \
    "$work/SPECS/ddccontrol.spec"
dnf builddep -y "$work/SPECS/ddccontrol.spec"
rpmbuild -ba --define "_topdir $work" "$work/SPECS/ddccontrol.spec"

arch=$(rpm --eval '%{_arch}')
release=$(rpm --eval '%{fedora}')
destination="$output/rpm/fedora/$release/$arch"
mkdir -p "$destination" "$output/srpm/fedora/$release/$arch"
find "$work/RPMS" -name '*.rpm' -exec cp -t "$destination" {} +
cp "$work/SRPMS"/*.src.rpm "$output/srpm/fedora/$release/$arch/"
