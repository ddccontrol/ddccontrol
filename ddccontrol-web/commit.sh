#!/bin/sh
echo "Making..."
make
echo "Cleaning..."
make clean
echo "Committing your changes to root files (message: '$1')..."
cvs commit -l -m "$1"
echo "Remaking..."
make
echo "Committing updated html files and other files (message: '$1')..."
cvs commit -m "$1"
echo "Updating website..."
make update
echo "OK"
