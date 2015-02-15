#!/bin/sh

set -e

echo "Running autoreconf..."
autoreconf --install $*

echo "Running intltoolize..."
intltoolize --force
