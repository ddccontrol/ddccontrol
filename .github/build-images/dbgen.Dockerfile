FROM rust:1.85.0-slim-bookworm AS rust
# Match the native Ubuntu environment used before containerizing this build.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive \
    RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo
ENV PATH=/usr/local/cargo/bin:$PATH
COPY --from=rust /usr/local/rustup /usr/local/rustup
COPY --from=rust /usr/local/cargo /usr/local/cargo
ARG TARGETARCH
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc libc6-dev git ca-certificates musl-tools binutils astyle jq && rm -rf /var/lib/apt/lists/*
# A successful link is insufficient: the bookworm x86_64 environment produced
# test executables that crashed at startup. Exercise the runtime before publishing.
RUN case "$TARGETARCH" in \
      amd64) target=x86_64-unknown-linux-musl ;; \
      arm64) target=aarch64-unknown-linux-musl ;; \
      *) exit 1 ;; \
    esac && rustup target add --toolchain 1.85.0 "$target" \
    && printf '#[test]\nfn starts() {}\n' > /tmp/musl-smoke.rs \
    && rustc +1.85.0 --test --target "$target" \
        -Ctarget-feature=+crt-static -Clinker=musl-gcc \
        /tmp/musl-smoke.rs -o /tmp/musl-smoke \
    && /tmp/musl-smoke && rm /tmp/musl-smoke /tmp/musl-smoke.rs
