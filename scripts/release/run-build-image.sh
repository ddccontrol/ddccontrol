#!/usr/bin/env bash
# Run release tools as the runner user, keeping artifacts and step outputs usable.
set -euo pipefail
kind=${1:?expected image kind}
platform=${2:?expected target platform}
shift 2
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
image=$("$root/scripts/release/build-image.sh" ensure "$kind" "$platform")
echo "Build host: $(uname -m); container target: $platform" >&2
docker run --rm --interactive --platform "$platform" --user "$(id -u):$(id -g)" \
    -v "$GITHUB_WORKSPACE:$GITHUB_WORKSPACE" -v "$RUNNER_TEMP:$RUNNER_TEMP" \
    -w "$PWD" -e HOME=/tmp -e CARGO_HOME=/tmp/cargo \
    -e EXPECTED_VERSION -e SOURCE_COMMIT -e RELEASE_VERSION -e DBGEN_RUST_TOOLCHAIN \
    -e CARGO -e RUSTC -e RUSTDOC -e GITHUB_OUTPUT -e RUNNER_TEMP \
    -e PACKAGE_SIGNING_KEY -e PACKAGE_SIGNING_PASSPHRASE -e PACKAGE_SIGNING_FINGERPRINT \
    -e PAGES_URL -e RELEASE_COMMIT -e BUILD_COMMIT \
    -e DEBIAN_PACKAGING_REF -e FEDORA_PACKAGING_REF \
    "$image" "$@"
