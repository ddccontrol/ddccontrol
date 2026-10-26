# syntax=docker/dockerfile:1
FROM fedora:44
RUN dnf install -y rpm-build dnf5-plugins
# The private packaging checkout is a build input, not an image layer.
RUN --mount=type=bind,source=ddccontrol.spec,target=/tmp/ddccontrol.spec \
    dnf builddep -y /tmp/ddccontrol.spec && dnf clean all
