FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

COPY install-rust.sh /usr/local/bin/ddccontrol-ci-install-rust

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        astyle \
        autoconf \
        build-essential \
        ca-certificates \
        cargo-1.77 \
        clang \
        dh-autoreconf \
        git \
        intltool \
        libgtk-3-dev \
        libpci-dev \
        libxml2-dev \
        pkg-config \
        rustc-1.77 \
        xsltproc \
        tidy \
        docbook-xml \
        docbook-xsl \
        xml-core \
    && ddccontrol-ci-install-rust \
    && rm -rf /var/lib/apt/lists/*
