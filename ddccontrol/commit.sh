#!/bin/sh
echo "Committing your changes (message: '$1')..."
cvs commit -m "$1"
echo "Updating ChangeLog..."
cvs2cl.pl --gmt -I ChangeLog
echo "Committing ChangeLog..."
cvs commit -m "Update ChangeLog" ChangeLog
echo "OK"
