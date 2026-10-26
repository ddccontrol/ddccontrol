FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    apt-utils dpkg-dev rpm createrepo-c gnupg dnf shellcheck \
    ca-certificates python3 jq git && rm -rf /var/lib/apt/lists/*
