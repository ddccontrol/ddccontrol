#!/bin/sh
echo "Committing your changes (message: '$1')..."
svn commit -m "$1"
echo "Updating ChangeLog..."
(TZ=GMT svn2cl -i -r COMMITTED --authors=AUTHORS --stdout; cat ChangeLog) >ChangeLog.tmp && mv -f ChangeLog.tmp ChangeLog
echo "Committing ChangeLog..."
svn commit -m "Update ChangeLog" ChangeLog
echo "OK"
