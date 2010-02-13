#!/bin/sh
echo "Updating working copy..."
svn update $SVN_OPTS || exit 1
echo "Committing your changes (message: '$1')..."
svn commit $SVN_OPTS -m "$1"
echo "Updating ChangeLog..."
svn update $SVN_OPTS
REVISION=$(svn info |grep ^Revision |cut -f2 -d' ')
(TZ=GMT svn2cl -i -r $REVISION --authors=AUTHORS --stdout; cat ChangeLog) >ChangeLog.tmp && mv -f ChangeLog.tmp ChangeLog
echo "Committing ChangeLog..."
svn commit $SVN_OPTS -m "Update ChangeLog" ChangeLog
echo "OK"
