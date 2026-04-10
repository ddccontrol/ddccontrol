FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update
RUN apt-get install -y \
    build-essential \
    astyle \
    autoconf \
    automake \
    dh-autoreconf \
    gettext \
    intltool \
    libglib2.0-dev \
    libgtk-3-dev \
    libpci-dev \
    libtool \
    libxml2-dev \
    pkg-config

RUN which make && make --version

WORKDIR /src

# Kopier inn hele repoet
COPY . /src

# Generer autotools-filer og bygg
RUN ./autogen.sh
RUN ./configure
RUN make -j"$(nproc)"

CMD ["/bin/bash"]
