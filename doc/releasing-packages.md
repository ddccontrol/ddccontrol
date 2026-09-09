# Release package repositories

The `Release packages` workflow builds Debian and Fedora packages from a
published stable DDCcontrol release and deploys signed repositories to
<https://ddccontrol.github.io/ddccontrol/>.

Release Please calls this workflow directly after creating a release. This is
necessary because releases created using `GITHUB_TOKEN` do not trigger another
`release` workflow. Manually published stable releases also trigger it. Drafts
and prereleases are excluded; tags must be `X.Y.Z` or `vX.Y.Z`, starting at 3.3.0.

## Maintainer setup

In **ddccontrol/ddccontrol → Settings → Pages**, select **GitHub Actions** as the
build source. Allow the `github-pages` environment to deploy from `master` and
release tags. This uses this repository's project site, independently of the
organization website in `ddccontrol/ddccontrol.github.io`.

Configure these repository Actions settings before publishing a release:

| Setting | Type | Value |
| --- | --- | --- |
| `PACKAGE_SIGNING_KEY` | Secret | ASCII-armored OpenPGP private signing key |
| `PACKAGE_SIGNING_PASSPHRASE` | Secret | Key passphrase; omit for an unencrypted key |
| `PACKAGE_SIGNING_FINGERPRINT` | Variable | Full 40-character signing-key fingerprint |
| `PACKAGING_READ_TOKEN` | Secret | Token with Contents: read on `ddccontrol/fedora-ddccontrol`, if that repo is private |

The built-in `GITHUB_TOKEN` cannot read a different private repository. The
Fedora checkout falls back to it when `PACKAGING_READ_TOKEN` is absent, which
also works if the packaging repository is made public. Credentials are not
persisted in the packaging checkouts or passed to build containers.

Generate a dedicated signing key on a trusted machine, keep a backup, and export
it to the Actions secret. For example:

```sh
gpg --quick-generate-key 'DDCcontrol packages <YOUR_EMAIL>' rsa4096 sign 2y
gpg --list-secret-keys --keyid-format long --with-fingerprint
gpg --armor --export-secret-keys YOUR_FULL_FINGERPRINT
```

Store the exported private key only in the secret. Publish the public fingerprint
in the project's release documentation so users can verify it. Renew the key
before it expires. The workflow exports its public part as `ddccontrol.asc`.
It does not generate a new production key on each run: clients must be able to
trust the same key across releases. Changing to a different key requires a
planned migration of client keyrings and retained RPM signatures.

## Build inputs and supported targets

| Packages | Build environment | Architectures |
| --- | --- | --- |
| Debian | Debian 13, `debian:trixie` | amd64, arm64, armhf, ppc64el, riscv64, s390x |
| RPM | Fedora 44, `fedora:44` | x86_64, aarch64 |

Native amd64 and arm64 runners build the corresponding packages. Other Debian
targets use QEMU and an explicit matching container platform. The matrix does
not change the package definitions' `Architecture: any` support.

The workflow pins packaging revisions from
[debian-ddccontrol](https://github.com/ddccontrol/debian-ddccontrol) and
[fedora-ddccontrol](https://github.com/ddccontrol/fedora-ddccontrol) in
`DEBIAN_PACKAGING_REF` and `FEDORA_PACKAGING_REF`. Review updates to these pins
together with the package builds. Source code comes from the resolved release
commit, rather than the packaging repositories' upstream snapshots.

The Debian recipe copies the external `debian/` directory and applies the
reviewed `scripts/release/debian.patch`. It adds Debian-packaged Rust build
dependencies, enables tests, includes the NVIDIA configuration file, and moves
the library package to `libddccontrol1` for SONAME 1. Breaks/Replaces handles files
previously owned by `libddccontrol0`. All binary packages use `Architecture: any`;
`dh_makeshlibs` and `dh_shlibdeps` run as part of every architecture build. The
builder also checks the resulting CLI and GUI dependencies on `libddccontrol1`.

Debian builds resolve Cargo's declared dependency ranges offline using
`/usr/share/cargo/registry`, regenerating the lockfile in the temporary build
tree. They do not fetch crates.io dependencies or use the vendored RPM source.
Fedora uses the external spec and the vendored source distribution, with the
release version substituted into the spec. Source RPMs are retained with the
binary packages. These are upstream release builds, not uploads to the official
Debian or Fedora archives.

## Publication and recovery

Build jobs do not receive signing secrets. Once all builds succeed, the signing
job signs binary and source RPMs and saves a
`ddccontrol-X.Y.Z-packages.tar.gz` release asset containing packages, Debian
build metadata, source RPMs and input commit IDs. The source tarballs remain
separate release assets. APT authenticates `.deb` files through the signed
index and its package hashes; individual `.deb` signatures are not required.

Every publication downloads package bundles from all stable releases and
rebuilds the complete repository. It generates `Packages`, compressed indexes,
`Release`, `Release.gpg` and `InRelease` for APT, and uses `createrepo_c` plus a
detached signature on `repomd.xml` for RPM. RPM signatures are verified before
indexing. Only then is the complete Pages artifact deployed.

Old package versions remain available. Publication is serialized and does not
depend on Actions artifact or cache retention. Do not delete package bundles
from releases if those versions should remain installable. The accumulated site
must stay within [GitHub Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits).

To recover a failed publication or publish an existing release, run **Actions →
Release packages → Run workflow**, selecting `master` and entering the release
tag. If its signed package bundle already exists, the workflow reuses it and
rebuilds Pages without replacing package files. A failure before the bundle was
saved rebuilds the packages. Rebuilding an older release cannot remove newer
versions because the repository is assembled from all bundles.

## Install from the repositories

Enable the repository matching the OS release. Debian trixie binaries are not
advertised as compatible with other Debian or Ubuntu releases. Fedora paths
include the release number so DNF will not use Fedora 44 binaries after an OS
upgrade to an unsupported Fedora release.

For Debian 13:

```sh
sudo install -d -m 0755 /etc/apt/keyrings
curl -fsSL https://ddccontrol.github.io/ddccontrol/ddccontrol.asc -o ddccontrol.asc
gpg --show-keys --with-fingerprint ddccontrol.asc
# Compare the fingerprint with the maintainer-published fingerprint first.
sudo install -m 0644 ddccontrol.asc /etc/apt/keyrings/ddccontrol.asc
echo 'deb [signed-by=/etc/apt/keyrings/ddccontrol.asc] https://ddccontrol.github.io/ddccontrol/apt trixie main' |
  sudo tee /etc/apt/sources.list.d/ddccontrol.list
sudo apt update
sudo apt install ddccontrol gddccontrol
```

For Fedora 44:

```sh
curl -fsSL https://ddccontrol.github.io/ddccontrol/ddccontrol.asc -o ddccontrol.asc
gpg --show-keys --with-fingerprint ddccontrol.asc
# Compare the fingerprint with the maintainer-published fingerprint first.
sudo rpm --import ddccontrol.asc
sudo curl -fsSL https://ddccontrol.github.io/ddccontrol/ddccontrol.repo \
  -o /etc/yum.repos.d/ddccontrol.repo
sudo dnf install ddccontrol ddccontrol-gtk
```

The monitor database (`ddccontrol-db`) remains a dependency supplied by the OS
repositories. The Fedora configuration enables both `gpgcheck=1` and
`repo_gpgcheck=1`.

## Validation

`Package repository checks` builds the PR's source and Debian packages on all
six target architectures using the same reusable build workflow as releases.
It also runs an integration test with a disposable,
passphrase-protected signing key. It checks separate architecture indexes,
retained older versions, installation with DNF, and APT download verification
including rejection of a modified package. Bundle tests reject traversal,
links and duplicate package paths. To run these checks in a disposable Linux
environment with the required tools installed:

```sh
shellcheck scripts/release/*.sh
PYTHONDONTWRITEBYTECODE=1 python3 scripts/release/test-bundles.py
sudo scripts/release/test-repositories.sh
```
