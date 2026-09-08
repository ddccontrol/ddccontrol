#!/usr/bin/env bash
# Integration test with disposable keys, packages and isolated client state.
# Requires apt-utils, dpkg-dev, rpm, createrepo-c, gnupg and dnf.
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
chmod 755 "$work"
export GNUPGHOME="$work/key"
mkdir -m 700 "$GNUPGHOME"
gpg --batch --pinentry-mode loopback --passphrase integration-test \
    --quick-generate-key 'DDCcontrol repository test <test@example.invalid>' rsa2048 sign 1d
export PACKAGE_SIGNING_KEY PACKAGE_SIGNING_PASSPHRASE=integration-test PACKAGE_SIGNING_FINGERPRINT
PACKAGE_SIGNING_FINGERPRINT=$(gpg --batch --with-colons --list-secret-keys | awk -F: '$1 == "fpr" {print $10; exit}')
PACKAGE_SIGNING_KEY=$(gpg --batch --pinentry-mode loopback --passphrase integration-test \
    --armor --export-secret-keys "$PACKAGE_SIGNING_FINGERPRINT")
gpgconf --kill gpg-agent
unset GNUPGHOME

for arch in amd64 arm64; do
    for version in 1.0.0 2.0.0; do
        directory="$work/packages/debian/trixie/$arch"
        mkdir -p "$directory" "$work/deb/DEBIAN" "$work/deb/usr/share/ddccontrol-repo-test"
        cat > "$work/deb/DEBIAN/control" <<EOF
Package: ddccontrol-repo-test
Version: $version
Architecture: $arch
Maintainer: Test <test@example.invalid>
Description: Disposable repository test
EOF
        printf '%s\n' "$version" > "$work/deb/usr/share/ddccontrol-repo-test/version"
        dpkg-deb --root-owner-group --build "$work/deb" "$directory/test_${version}_${arch}.deb"
    done
done

mkdir -p "$work/rpmbuild/SPECS"
cat > "$work/rpmbuild/SPECS/test.spec" <<'EOF'
Name: ddccontrol-repo-test
Version: 2.0.0
Release: 1
Summary: Disposable repository test
License: MIT
BuildArch: noarch
%description
Disposable repository test.
%install
mkdir -p %{buildroot}/usr/share/ddccontrol-repo-test
echo repository-test > %{buildroot}/usr/share/ddccontrol-repo-test/version
%files
/usr/share/ddccontrol-repo-test/version
EOF
rpmbuild -bb --define "_topdir $work/rpmbuild" "$work/rpmbuild/SPECS/test.spec"
mkdir -p "$work/packages/rpm/fedora/44/x86_64"
cp "$work/rpmbuild/RPMS/noarch"/*.rpm "$work/packages/rpm/fedora/44/x86_64/"
(
    # shellcheck source=scripts/release/sign-packages.sh
    source "$script_dir/sign-packages.sh"
    sign_rpm "$work/packages/rpm/fedora/44/x86_64/ddccontrol-repo-test-2.0.0-1.noarch.rpm"
)
"$script_dir/create-repositories.sh" "$work/packages" "$work/site" https://example.invalid/ddccontrol

mkdir -p "$work/apt/lists/partial" "$work/apt/cache/archives/partial"
printf 'deb [signed-by=%s] file:%s trixie main\n' \
    "$work/site/ddccontrol.asc" "$work/site/apt" > "$work/apt/sources.list"
apt_options=(
    -o "Dir::Etc::sourcelist=$work/apt/sources.list"
    -o Dir::Etc::sourceparts=-
    -o "Dir::State::lists=$work/apt/lists"
    -o "Dir::Cache=$work/apt/cache"
    -o "APT::Sandbox::User=$(id -un)"
    -o APT::Architecture=amd64
    -o APT::Architectures=amd64
)
apt-get "${apt_options[@]}" update
apt-cache "${apt_options[@]}" policy ddccontrol-repo-test | grep 'Candidate: 2.0.0' > /dev/null
# Both old versions and the foreign architecture must remain available.
grep -q 'Version: 1.0.0' "$work/site/apt/dists/trixie/main/binary-amd64/Packages"
grep -q 'Architecture: arm64' "$work/site/apt/dists/trixie/main/binary-arm64/Packages"
if grep -q 'Architecture: arm64' "$work/site/apt/dists/trixie/main/binary-amd64/Packages"; then
    echo 'An architecture index contains foreign packages' >&2
    exit 1
fi
cd "$work"
apt-get "${apt_options[@]}" download ddccontrol-repo-test=2.0.0
rm ddccontrol-repo-test_*.deb
printf tampered >> "$work/site/apt/pool/trixie/main/d/ddccontrol/test_2.0.0_amd64.deb"
if apt-get "${apt_options[@]}" download ddccontrol-repo-test=2.0.0; then
    echo 'APT accepted a modified package' >&2
    exit 1
fi

dnf --installroot "$work/dnf" --releasever 44 --disablerepo '*' \
    --repofrompath "ddccontrol-test,file://$work/site/rpm/fedora/44/x86_64" \
    --setopt=ddccontrol-test.gpgcheck=1 --setopt=ddccontrol-test.repo_gpgcheck=1 \
    --setopt="ddccontrol-test.gpgkey=file://$work/site/ddccontrol.asc" \
    -y install ddccontrol-repo-test
test -f "$work/dnf/usr/share/ddccontrol-repo-test/version"
echo 'APT and DNF accepted the signed repositories; APT rejected a modified package.'
