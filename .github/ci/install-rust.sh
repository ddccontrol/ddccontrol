#!/bin/sh

# Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

# Select the same Ubuntu toolchain in fresh and cached CI images.
set -eu

if ! command -v cargo-1.77 >/dev/null 2>&1 || ! command -v rustc-1.77 >/dev/null 2>&1; then
    apt-get update
    apt-get install -y --no-install-recommends cargo-1.77 rustc-1.77
fi

ln -sf /usr/bin/cargo-1.77 /usr/local/bin/cargo
ln -sf /usr/bin/rustc-1.77 /usr/local/bin/rustc
ln -sf /usr/bin/rustdoc-1.77 /usr/local/bin/rustdoc
cargo --version
rustc --version
rustdoc --version
