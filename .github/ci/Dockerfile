FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        astyle \
        autoconf \
        build-essential \
        ca-certificates \
        cargo \
        clang \
        dh-autoreconf \
        git \
        intltool \
        libgtk-3-dev \
        libpci-dev \
        libxml2-dev \
        pkg-config \
        rustc \
        xsltproc \
        tidy \
        docbook-xml \
        docbook-xsl \
        xml-core \
    && rm -rf /var/lib/apt/lists/*
