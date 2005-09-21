#!/bin/sh

#echo "no" | gettextize --copy --force --no-changelog

echo "Running intltoolize..."
intltoolize --copy --force --automake

echo "Running aclocal..."
aclocal

echo "Running autoconf..."
autoconf

echo "Running automake..."
automake -a
