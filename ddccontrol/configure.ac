AC_INIT([DDC/CI control tool], [0.1],
        [Nicolas Boichat <nicolas@boichat.ch>],
        [ddccontrol])
AM_CONFIG_HEADER(src/config.h)
AM_INIT_AUTOMAKE

# Avoid automatic *.in file regeneration
AM_MAINTAINER_MODE

AM_GNU_GETTEXT([external])

AC_PROG_RANLIB
AC_PROG_CC
AC_PROG_INSTALL

AC_LANG([C])

AC_STDC_HEADERS
AC_CHECK_HEADERS([stdio.h unistd.h sys/time.h stdlib.h errno.h unistd.h string.h sys/types.h sys/stat.h fcntl.h sys/ioctl.h dirent.h], [], [AC_MSG_ERROR([Important header not found, please install it.], [1])], [])
# AC_CHECK_HEADERS([linux/i2c-dev.h], [], [AC_MSG_ERROR([Linux kernel header files not found, please install them.], [1])], [])

support_i2c_dev=no
AC_MSG_CHECKING([if linux/i2c-dev.h works alone])
AC_COMPILE_IFELSE([
[#include <linux/i2c-dev.h>]
[const int a = I2C_RDWR + I2C_M_RD;]
[struct i2c_msg i2cmsg;]
],
support_i2c_dev=yes
AC_MSG_RESULT(yes),
AC_MSG_RESULT(no)
)

if test "x$support_i2c_dev" = "xno" ; then
  AC_MSG_CHECKING([if linux/i2c-dev.h works with some tweaks])
  AC_COMPILE_IFELSE([
  [#define __user]
  [#define s32 int]
  [#include <linux/i2c.h>]
  [#include <linux/i2c-dev.h>]
  [const int a = I2C_RDWR + I2C_M_RD;]
  [struct i2c_msg i2cmsg;]
  ],
  AC_DEFINE(HAVE_BUGGY_I2C_DEV, 1, [if linux/i2c-dev.h is buggy on your system, but could be workarounded])
  support_i2c_dev=yes
  AC_MSG_RESULT(yes),
  AC_MSG_RESULT(no)
  )
  if test "x$support_i2c_dev" = "xno" ; then
   AC_MSG_ERROR([Unable to use linux/i2c-dev.h on your system, this header may be more buggy than usual, or inexistant, try to (re-)install kernel headers.], [1])
  fi
fi

AC_DEFINE(_GNU_SOURCE, 1, [Define unconditionally for setting a GNU environment.])

# libxml2 check

AC_PATH_PROG(XML2_CONFIG, xml2-config, no)
if test "x$XML2_CONFIG" = "xno" ; then
   AC_MSG_ERROR([xml2-config not found, please install libxml2, available at http://www.xmlsoft.org/.], [1])
fi

CFLAGS="$CFLAGS -Wall `xml2-config --cflags` -DDATADIR=\"\\\"${datadir}/ddccontrol-db\\\"\" -DBINDIR=\"\\\"${bindir}\\\"\""
LDFLAGS="$LDFLAGS `xml2-config --libs`"

# Direct PCI memory access check
support_ddcpci=yes
AC_ARG_ENABLE(ddcpci,
  [  --disable-ddcpci	  enable direct PCI memory access (enabled) ],
  [if test x$enableval = xno; then
    support_ddcpci=no
  fi])

DDCPCI=
ddcpci_val=0
if test x$support_ddcpci = xyes; then
   AC_CHECK_HEADERS([pci/pci.h], [], [AC_MSG_ERROR([PCI utils headers not found, please install pci-utils.], [1])], [])
   AC_CHECK_LIB([pci], [pci_alloc], [], [AC_MSG_ERROR([PCI utils library not found, please install pci-utils.], [1])])
   DDCPCI=ddcpci
   ddcpci_val=1
fi
AC_DEFINE_UNQUOTED(HAVE_DDCPCI, $ddcpci_val, [Define if ddccontrol is built with ddcpci support.])

AC_SUBST([DDCPCI])

# Gnome check

AC_ARG_ENABLE(gnome,
  [  --enable-gnome	  enable build of gnome-console GUI (disabled)],
  [if test x$enableval = xyes; then
    support_gnome=yes
  fi])

GDDCCONTROL=
if test x$support_gnome = xyes; then
   AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
   if test "x$PKG_CONFIG" = "xno" ; then
      AC_MSG_ERROR(pkg-config not found, please install pkg-config)
   fi

   echo -n "checking for gtk+-2.0... "
   if pkg-config --exists gtk+-2.0 ; then
      GNOME_LDFLAGS="`pkg-config --libs gtk+-2.0`"
      GNOME_CFLAGS="$CFLAGS `pkg-config --cflags gtk+-2.0`"
      GDDCCONTROL=gddccontrol
      echo "yes"
   else
      echo "no"
   fi
fi

AC_SUBST([GNOME_LDFLAGS])
AC_SUBST([GNOME_CFLAGS])
AC_SUBST([GDDCCONTROL])

AC_CONFIG_FILES([ 
    po/Makefile.in
	Makefile
	src/Makefile
	src/lib/Makefile 
	src/ddccontrol/Makefile
	src/gddccontrol/Makefile
	src/ddcpci/Makefile])
AC_OUTPUT
