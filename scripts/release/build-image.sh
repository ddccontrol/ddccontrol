#!/usr/bin/env bash
# Share recipe-derived image tags between consumers and the image publisher.
set -euo pipefail
mode=${1:?expected ref, ensure or publish}
kind=${2:?expected source, debian, fedora, dbgen or repositories}
platform=${3:?expected target platform}
packaging=${4:-}
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
case "$platform" in
    linux/amd64) arch=amd64 ;;
    linux/arm64) arch=arm64 ;;
    linux/arm/v7) arch=armhf ;;
    linux/ppc64le) arch=ppc64el ;;
    linux/riscv64) arch=riscv64 ;;
    linux/s390x) arch=s390x ;;
    *) echo "Unsupported build platform: $platform" >&2; exit 1 ;;
esac
context=$(mktemp -d)
trap 'rm -rf "$context"' EXIT
case "$kind" in
    source)
        cp "$root/.github/ci/Dockerfile" "$context/Dockerfile"
        cp "$root/.github/ci/install-rust.sh" "$context/"
        ;;
    debian)
        cp "$root/.github/build-images/debian.Dockerfile" "$context/Dockerfile"
        cp -a "${packaging:?expected Debian packaging checkout}/debian" "$context/debian"
        (cd "$context" && patch --quiet --fuzz=0 -p1 < "$root/scripts/release/debian.patch") >&2
        mv "$context/debian/control" "$context/control"
        rm -rf "$context/debian"
        ;;
    fedora)
        cp "$root/.github/build-images/fedora.Dockerfile" "$context/Dockerfile"
        cp "${packaging:?expected Fedora packaging checkout}/ddccontrol.spec" "$context/"
        ;;
    dbgen|repositories)
        cp "$root/.github/build-images/$kind.Dockerfile" "$context/Dockerfile"
        ;;
    *) echo "Unknown build image: $kind" >&2; exit 1 ;;
esac
# Hash names as well as contents, with a fixed sort order. No git checkout is
# sent to Docker; Fedora credentials and packaging sources stay out of layers.
key=$(python3 - "$context" "${BASH_SOURCE[0]}" <<'PYHASH'
import hashlib
import pathlib
import sys
h = hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes())
for path in sorted(pathlib.Path(sys.argv[1]).iterdir()):
    h.update(path.name.encode() + b'\0' + path.read_bytes() + b'\0')
print(h.hexdigest()[:32])
PYHASH
)
image="ghcr.io/ddccontrol/ddccontrol-ci:release-$kind-$key-$arch"
if [[ "$mode" == ref ]]; then
    echo "$image"
    exit
fi
echo "Build host: $(uname -m); image target: $platform" >&2
case "$mode" in
    ensure)
        if ! docker pull --platform "$platform" "$image" >&2; then
            echo "Prebuilt image unavailable; building the matching recipe locally." >&2
            docker build --platform "$platform" --tag "$image" "$context" >&2
        fi
        ;;
    publish)
        docker buildx build --platform "$platform" --pull \
            --label "org.opencontainers.image.source=https://github.com/ddccontrol/ddccontrol" \
            --tag "$image" --push "$context" >&2
        ;;
    *) echo "Unknown mode: $mode" >&2; exit 1 ;;
esac
echo "$image"
