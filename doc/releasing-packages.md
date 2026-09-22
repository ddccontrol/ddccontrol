# Release package repositories

`Release Please` is the only workflow that starts real Debian and Fedora
package builds. Ordinary PRs run the normal C/Rust CI and lightweight repository
tests; they do not run the package architecture matrices.

Release Please first creates or updates its release PR without publishing a
release. It then checks the source distributions and builds all six Debian and
both Fedora targets from that PR's exact head commit. It also checks the monitor
database and signed repository generation. The `Release Please package build`
check on the release PR reports their combined result. A failed or cancelled
package build fails the Release Please workflow and prevents publication.

This validation is called directly after updating the release PR, because PRs
created with `GITHUB_TOKEN` do not trigger `pull_request` workflows. Human updates
to the same trusted release PR also start validation. Normal PRs, forks and
unlabelled branches cannot start this privileged validation.

Before merging the release PR, wait for its package check and the associated
Release Please run to finish successfully. After merge, the publication gate
requires that current successful check, its completed workflow run, every
unexpired package artifact, and identical source trees for the tested PR and
merged release commit. Missing or stale results fail closed: no tag or GitHub
release is created. The `always-update` Release Please setting refreshes the release PR even when
only hidden changelog entries change. Keep it up to date with `master` before
merging.

Only then does Release Please create the release. The `Release packages`
reusable workflow downloads the approved build's source and package artifacts,
signs the packages and deploys the signed repositories to
<https://ddccontrol.github.io/ddccontrol/>. It does not rebuild the packages after
merge. Signing and Pages deployment failures still fail the publishing run and
can be retried separately. Stable tags must be `X.Y.Z` or `vX.Y.Z`, starting at
3.3.0. There is no separate automatic `release: published` package build.

## Maintainer setup

In **ddccontrol/ddccontrol → Settings → Pages**, select **GitHub Actions** as the
build source. Allow the `github-pages` environment to deploy from `master` and
release tags. This uses this repository's project site, independently of the
organization website in `ddccontrol/ddccontrol.github.io`.

In **ddccontrol → Settings → Secrets and variables → Actions**, configure these
organization settings before publishing a release. For each secret and variable,
choose **Selected repositories** and grant access to **ddccontrol/ddccontrol**,
where the release workflow runs. The default **Private repositories** visibility
does not include this public repository.

| Setting | Type | Value |
| --- | --- | --- |
| `DDCCONTROL_PACKAGE_SIGNING_KEY` | Secret | ASCII-armored OpenPGP private signing key |
| `DDCCONTROL_PACKAGE_SIGNING_PASSPHRASE` | Secret | Key passphrase; omit for an unencrypted key |
| `DDCCONTROL_PACKAGE_SIGNING_FINGERPRINT` | Variable | Full 40-character signing-key fingerprint |
| `FEDORA_DDCCONTROL_READ_TOKEN` | Secret | Fine-grained personal access token with Contents: read on `ddccontrol/fedora-ddccontrol`, if that repo is private |

For the read token, select `ddccontrol` as the resource owner and limit repository
access to `fedora-ddccontrol`. Its Contents permission needs only read access.
The token's repository access and the organization secret's repository access
serve different purposes: the token reads the Fedora packaging repository,
while the secret is available to the workflow in `ddccontrol/ddccontrol`.

The built-in `GITHUB_TOKEN` cannot read a different private repository. The
Fedora checkout falls back to it when `FEDORA_DDCCONTROL_READ_TOKEN` is absent, which
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
together with the package builds. Source code comes from the tested release PR head. Publication verifies its
Git tree against the resolved release commit, and records both commit IDs in
the package provenance. Recovery builds use the resolved release commit.

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
Release Please → Run workflow**, selecting `master` and entering the existing release
tag. If its signed package bundle already exists, the workflow reuses it and
rebuilds Pages without replacing package files. A failure before the bundle was
saved rebuilds the packages. Rebuilding an older release cannot remove newer
versions because the repository is assembled from all bundles.

Leave the tag input empty to prepare or validate the pending release PR. If a
release PR was merged before validation completed, wait for the validation run
to finish and run Release Please again with an empty tag input. A source-tree
mismatch requires a corrected release PR and a new successful build. If an
artifact expires before merge, rerun validation on the open release PR.
The manual tag input only accepts an already published release, so it cannot
bypass the gate to create a new release.

During recovery, existing source archives are downloaded and reused byte for byte, with size and
available SHA-256 digests checked before building packages. The source build is
skipped when both archives are already present. Missing archives and package
bundles are uploaded directly as release assets; publication does not edit
release metadata or replace existing assets. After merging a workflow fix,
start a new run on `master`; re-running an old run uses its original workflow.

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

`Package repository checks` runs release-gate regression tests, script checks,
bundle extraction tests and a small repository integration test. It creates
synthetic test packages with a disposable,
passphrase-protected signing key. It checks separate architecture indexes,
retained older versions, installation with DNF, and APT download verification
including rejection of a modified package. Bundle tests reject traversal,
links and duplicate package paths. To run these checks in a disposable Linux
environment with the required tools installed:

```sh
shellcheck scripts/release/*.sh
node --test scripts/release/test-*.cjs
PYTHONDONTWRITEBYTECODE=1 python3 scripts/release/test-bundles.py
sudo scripts/release/test-repositories.sh
```
