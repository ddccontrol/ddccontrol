#!/bin/sh
echo "Updating working copy..."
svn update || exit 1
echo "Committing your changes (message: '$1')..."
svn commit -m "$1"
echo "Updating ChangeLog..."
svn update
REVISION=$(svn info |grep ^Revision |cut -f2 -d' ')
(TZ=GMT svn2cl -i -r $REVISION --authors=AUTHORS --stdout; cat ChangeLog) >ChangeLog.tmp && mv -f ChangeLog.tmp ChangeLog
echo "Committing ChangeLog..."
svn commit -m "Update ChangeLog" ChangeLog
echo "OK"
