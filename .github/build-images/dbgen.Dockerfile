FROM rust:1.85.0-slim-bookworm
ARG TARGETARCH
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc libc6-dev git ca-certificates musl-tools binutils astyle jq && rm -rf /var/lib/apt/lists/*
RUN case "$TARGETARCH" in \
      amd64) target=x86_64-unknown-linux-musl ;; \
      arm64) target=aarch64-unknown-linux-musl ;; \
      *) exit 1 ;; \
    esac && rustup target add --toolchain 1.85.0 "$target"
