#!/usr/bin/env bash
# Generate a complete Pages tree from the retained release package bundles.
set -euo pipefail

packages=$(realpath "$1")
site=$(realpath -m "$2")
base_url=${3%/}
[[ $base_url =~ ^https://[a-zA-Z0-9./_-]+$ ]]
test ! -e "$site"
mkdir -p "$site"
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=scripts/release/sign-packages.sh
source "$script_dir/sign-packages.sh"
export_public_key "$site/ddccontrol.asc"
mkdir -p "$GNUPGHOME/rpmdb"
rpm --dbpath "$GNUPGHOME/rpmdb" --import "$site/ddccontrol.asc"

shopt -s nullglob
for distribution in "$packages"/debian/*; do
    suite=$(basename "$distribution")
    apt_root="$site/apt"
    pool="pool/$suite/main/d/ddccontrol"
    mkdir -p "$apt_root/$pool"
    architectures=()
    for directory in "$distribution"/*; do
        arch=$(basename "$directory")
        architectures+=("$arch")
        cp "$directory"/*.deb "$apt_root/$pool/"
    done
    cd "$apt_root"
    for arch in "${architectures[@]}"; do
        index="dists/$suite/main/binary-$arch/Packages"
        mkdir -p "$(dirname "$index")"
        apt-ftparchive --arch "$arch" packages "$pool" > "$index"
        test -s "$index"
        gzip -n -9 -c "$index" > "$index.gz"
        xz -9 -c "$index" > "$index.xz"
    done
    release="dists/$suite/Release"
    apt-ftparchive \
        -o APT::FTPArchive::Release::Origin=DDCcontrol \
        -o APT::FTPArchive::Release::Label=DDCcontrol \
        -o "APT::FTPArchive::Release::Suite=$suite" \
        -o "APT::FTPArchive::Release::Codename=$suite" \
        -o "APT::FTPArchive::Release::Architectures=${architectures[*]}" \
        -o APT::FTPArchive::Release::Components=main \
        release "dists/$suite" > "$GNUPGHOME/Release"
    mv "$GNUPGHOME/Release" "$release"
    sign_file --armor --detach-sign --output "$release.gpg" "$release"
    sign_file --clearsign --output "dists/$suite/InRelease" "$release"
    gpg --verify "$release.gpg" "$release"
done

for directory in "$packages"/rpm/fedora/*/*; do
    release=$(basename "$(dirname "$directory")")
    arch=$(basename "$directory")
    destination="$site/rpm/fedora/$release/$arch"
    mkdir -p "$destination/Packages"
    cp "$directory"/*.rpm "$destination/Packages/"
    # Verify before indexing. The RPMs were signed before their bundle was saved.
    for package in "$destination"/Packages/*.rpm; do
        rpm --dbpath "$GNUPGHOME/rpmdb" --checksig "$package" | grep 'signatures OK' > /dev/null
    done
    createrepo_c --checksum sha256 "$destination"
    sign_file --armor --detach-sign "$destination/repodata/repomd.xml"
    gpg --verify "$destination/repodata/repomd.xml.asc" "$destination/repodata/repomd.xml"
done

cat > "$site/ddccontrol.repo" <<EOF
[ddccontrol]
name=DDCcontrol for Fedora \$releasever - \$basearch
baseurl=$base_url/rpm/fedora/\$releasever/\$basearch
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=$base_url/ddccontrol.asc
EOF
cat > "$site/index.html" <<EOF
<!doctype html>
<html lang="en"><meta charset="utf-8"><title>DDCcontrol package repositories</title>
<h1>DDCcontrol package repositories</h1>
<p>Signed packages for Debian 13 (trixie) and Fedora 44.</p>
<p><a href="https://github.com/ddccontrol/ddccontrol/blob/master/doc/releasing-packages.md">Installation and repository documentation</a></p>
<p><a href="ddccontrol.asc">Signing key</a>: <code>$PACKAGE_SIGNING_FINGERPRINT</code></p>
<p><a href="ddccontrol.repo">Fedora repository configuration</a></p>
</html>
EOF
touch "$site/.nojekyll"
