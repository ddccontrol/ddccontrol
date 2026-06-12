AC_INIT([DDC/CI control tool],[3.1.0],[https://github.com/ddccontrol/ddccontrol/issues],[ddccontrol]) dnl x-release-please-version
AC_CONFIG_SRCDIR([src/ddccontrol/main.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([subdir-objects -Wall foreign])

IT_PROG_INTLTOOL
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.21])
GETTEXT_PACKAGE=ddccontrol
AC_SUBST([GETTEXT_PACKAGE])
AS_IF([test "x$USE_NLS" = "xyes"],
      [RUST_GETTEXT_FEATURE="--features gettext"],
      [RUST_GETTEXT_FEATURE=""])
AC_SUBST([RUST_GETTEXT_FEATURE])

AC_PROG_CC
AC_PATH_PROG([CARGO], [cargo], [no])
AS_IF([test "x$CARGO" = "xno"],
      [AC_MSG_ERROR([cargo is required to build the Rust CAPS parser])])
AM_PROG_AR
AC_PROG_INSTALL
AC_PROG_MKDIR_P
LT_INIT
PKG_PROG_PKG_CONFIG

AC_LANG([C])

AC_CHECK_HEADERS([stdio.h unistd.h sys/time.h stdlib.h errno.h unistd.h string.h sys/types.h sys/stat.h fcntl.h sys/ioctl.h dirent.h], [], [AC_MSG_ERROR([Important header not found, please install it.], [1])], [])

check_xsltproc=yes
AC_ARG_ENABLE(xsltproc-check,
  [AS_HELP_STRING([--disable-xsltproc-check],[omit check to see if xsltproc works (enabled)])],
  [if test x$enableval = xno; then
    check_xsltproc=no
  fi])

# /dev/i2c-* check
support_i2c_dev=yes
AC_ARG_ENABLE(i2cdev,
  [AS_HELP_STRING([--disable-i2c],[enable /dev/i2c-* busses (enabled)])],
  [if test x$enableval = xno; then
    support_i2c_dev=no
  fi])

if test "x$support_i2c_dev" = "xyes" ; then
	AC_DEFINE_UNQUOTED(HAVE_I2C_DEV, 1, [Define if ddccontrol is built with /dev/i2c-* support.])

	AC_CANONICAL_HOST
	case $host_os in
		linux*)
		AC_CHECK_HEADERS([linux/types.h], [], [AC_MSG_ERROR([Linux kernel header files not found, please install them.], [1])], [])
		;;
	esac

	support_i2c_dev_works=no
	AC_MSG_CHECKING([if linux/i2c-dev.h works with non-kernel linux/types.h])
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
	#include <linux/types.h>
	#include <linux/i2c-dev.h>
	const int a = I2C_RDWR + I2C_M_RD;
	struct i2c_msg i2cmsg;
	]])],
	support_i2c_dev_works=yes
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
	)

	if test "x$support_i2c_dev_works" = "xno" ; then
	  AC_MSG_CHECKING([if $srcdir/src/lib/i2c-dev.h works instead])
	  AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
	  #define HAVE_BUGGY_I2C_DEV 1
	  #include "$srcdir/src/lib/i2c-dev.h"
	  const int a = I2C_RDWR + I2C_M_RD;
	  struct i2c_msg i2cmsg;
	  ]])],
	  AC_DEFINE(HAVE_BUGGY_I2C_DEV, 1, [Define if linux/i2c-dev.h is buggy on your system, so defaults constants and structures must be used])
	  support_i2c_dev_works=yes
	  AC_MSG_RESULT(yes),
	  AC_MSG_RESULT(no)
	  )
	  if test "x$support_i2c_dev_works" = "xno" ; then
	   AC_MSG_ERROR([Unable to use neither linux/i2c-dev.h nor src/lib/i2c-dev.h on your system, kernel headers may be more buggy than usual, or inexistant, try to (re-)install kernel headers.], [1])
	  fi
	fi
fi

AC_DEFINE(_GNU_SOURCE, 1, [Define unconditionally for setting a GNU environment.])

CFLAGS="$CFLAGS -Wall  -DDATADIR=\"\\\"${datadir}/ddccontrol-db\\\"\" -DBINDIR=\"\\\"${bindir}\\\"\""


# Check for extra libraries

AC_SEARCH_LIBS([round],[m])

# libxml2 check
PKG_CHECK_MODULES([LIBXML2], [libxml-2.0])

# Gnome check
support_gnome=yes
AC_ARG_ENABLE(gnome,
  AS_HELP_STRING([--disable-gnome],[disable build of gnome-console GUI (enabled)]),
  [if test x$enableval = xno; then
    support_gnome=no
  fi])

# This should use PKG_CHECK_MODULES() instead of explicitly calling $PKG_CONFIG.
GDDCCONTROL=
if test x$support_gnome = xyes; then
   if test "x$PKG_CONFIG" = "xno" ; then
      AC_MSG_ERROR(pkg-config not found, please install pkg-config)
   fi

   echo -n "checking for gtk+>=3.22... "
   if $PKG_CONFIG --atleast-version=3.22 gtk+-3.0 ; then
      GNOME_LDFLAGS="$LIBXML2_LIBS `$PKG_CONFIG --libs gtk+-3.0`"
      GNOME_CFLAGS="$LIBXML2_CFLAGS `$PKG_CONFIG --cflags gtk+-3.0`"
      GDDCCONTROL=gddccontrol
      echo "yes"

      # Check for X11
      AC_PATH_XTRA
   else
      echo "no"
      AC_MSG_ERROR([gtk+>=3.22 development packages not found])
   fi
fi

AC_SUBST([GNOME_LDFLAGS])
AC_SUBST([GNOME_CFLAGS])
AC_SUBST([GDDCCONTROL])

PKG_CHECK_MODULES([GIO_UNIX], [gio-unix-2.0 >= 2.36.0],[CFLAGS="${GIO_UNIX_CFLAGS} ${CFLAGS}" LIBS="${GIO_UNIX_LIBS} ${LIBS}"])

# Doc check
support_doc=no
AC_ARG_ENABLE(doc,
  AS_HELP_STRING([--enable-doc],[enable build of the documentation (disabled)]),
  [if test x$enableval = xyes; then
    support_doc=yes
  fi])

AM_CONDITIONAL(BUILD_DOC, test x$support_doc = xyes)

DOC=
if test x$support_doc = xyes; then
   # Docbook tests
   # Based on http://www.movement.uklinux.net/docs/docbook-autotools/configure.html
   # Copyright � 2003 John Levon
   # It's just rude to go over the net to build
   XSLTPROC_FLAGS=--nonet

   AC_MSG_CHECKING([whether /etc/xml/catalog exists])
   if test ! -f /etc/xml/catalog; then
      AC_MSG_ERROR([/etc/xml/catalog not found, fix this problem or disable doc building])
   else
      AC_MSG_RESULT(yes)
      AC_PATH_PROG(XSLTPROC,xsltproc,no)
      if test "x$XSLTPROC" = "xno" ; then
         AC_MSG_ERROR([xsltproc not found, install it or disable doc building])
      fi

      AC_PATH_PROG(SSH,ssh,ssh)
      AC_PATH_PROG(SCP,scp,scp)
      AC_PATH_PROG(ASPELL,aspell,no)
      AC_PATH_PROG(FOP,fop,no)

      AC_PATH_PROG(TIDY,tidy,no)
      if test "x$TIDY" = "xno" ; then
         AC_MSG_ERROR([tidy not found, install it or disable doc building])
      fi

      if test x$check_xsltproc = xyes ; then
         AC_MSG_CHECKING([whether xsltproc works])

         DOCBOOK_ROOT="$srcdir/doc"
         DB_FILE="$DOCBOOK_ROOT/style.xsl"

         $XSLTPROC $XSLTPROC_FLAGS $DB_FILE >/dev/null 2>&1 << END
<?xml version="1.0" encoding='ISO-8859-1'?>
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.3//EN" "http://www.oasis-open.org/docbook/xml/4.3/docbookx.dtd">
<book id="test">
</book>
END
         if test "$?" = 0; then
            AC_MSG_RESULT(yes)
            DOC=doc
         else
            AC_MSG_ERROR([xsltproc does not work, fix this problem or disable doc building])
         fi
      fi
   fi
fi

AC_SUBST(XML_CATALOG)
AC_SUBST(XSLTPROC_FLAGS)
AC_SUBST(DOCBOOK_ROOT)
AC_SUBST(DOC)


dnl ---------------------------------------------------------------------------
dnl - Select whether and where to install systemd system service files
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([systemdsystemunitdir],
            AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
            [],
            [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != "xno"; then
	AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$systemdsystemunitdir"])


AC_CONFIG_FILES([
	po/Makefile.in
	Makefile
	data/Makefile
	src/Makefile
	src/lib/Makefile
	src/lib/ddccontrol.pc
	src/daemon/Makefile
	src/daemon/data/Makefile
	src/ddccontrol/Makefile
	src/gddccontrol/Makefile
	doc/Makefile
	doc/main.xml
	man/Makefile])
AC_OUTPUT
