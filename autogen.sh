#!/bin/sh

set -e

echo "Running autoreconf..."
autoreconf --force --install $*

echo "Running intltoolize..."
intltoolize --force
