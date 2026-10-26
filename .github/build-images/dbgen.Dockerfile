FROM rust:1.85.0-slim-bookworm
ARG TARGETARCH
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc libc6-dev git ca-certificates binutils astyle jq && rm -rf /var/lib/apt/lists/*
# Use Rust's bundled musl CRT with the default native linker. Debian's
# musl-gcc wrapper breaks x86_64 static PIE startup. Execute a test harness
# before publishing so a successful link alone cannot hide this regression.
RUN case "$TARGETARCH" in \
      amd64) target=x86_64-unknown-linux-musl ;; \
      arm64) target=aarch64-unknown-linux-musl ;; \
      *) exit 1 ;; \
    esac && rustup target add --toolchain 1.85.0 "$target" \
    && printf '#[test]\nfn starts() {}\n' > /tmp/musl-smoke.rs \
    && rustc +1.85.0 --test --target "$target" \
        -Ctarget-feature=+crt-static -Clink-self-contained=yes \
        /tmp/musl-smoke.rs -o /tmp/musl-smoke \
    && /tmp/musl-smoke && rm /tmp/musl-smoke /tmp/musl-smoke.rs
