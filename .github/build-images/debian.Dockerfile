FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates devscripts equivs patch python3
# This is the pinned Debian control file after applying our packaging patch.
COPY control /tmp/control
RUN cd /tmp && mk-build-deps --install --remove \
    --tool 'apt-get -y --no-install-recommends' control \
    && rm -f /tmp/control && rm -rf /var/lib/apt/lists/*
