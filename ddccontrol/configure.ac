AC_INIT([DDC/CI control tool], [0.1.1],
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
  AC_CHECK_HEADERS([linux/types.h], [], [AC_MSG_ERROR([Linux kernel header files not found, please install them.], [1])], [])
  AC_MSG_CHECKING([if src/lib/i2c-dev.h works instead])
  AC_COMPILE_IFELSE([
  [#define HAVE_BUGGY_I2C_DEV 1]
  [#include "src/lib/i2c-dev.h"]
  [const int a = I2C_RDWR + I2C_M_RD;]
  [struct i2c_msg i2cmsg;]
  ],
  AC_DEFINE(HAVE_BUGGY_I2C_DEV, 1, [if linux/i2c-dev.h is buggy on your system, so defaults constants and structures must be used])
  support_i2c_dev=yes
  AC_MSG_RESULT(yes),
  AC_MSG_RESULT(no)
  )
  if test "x$support_i2c_dev" = "xno" ; then
   AC_MSG_ERROR([Unable to use neither linux/i2c-dev.h nor src/lib/i2c-dev.h on your system, kernel headers may be more buggy than usual, or inexistant, try to (re-)install kernel headers.], [1])
  fi
fi

AC_DEFINE(_GNU_SOURCE, 1, [Define unconditionally for setting a GNU environment.])

CFLAGS="$CFLAGS -Wall  -DDATADIR=\"\\\"${datadir}/ddccontrol-db\\\"\" -DBINDIR=\"\\\"${bindir}\\\"\""

# libxml2 check

AC_PATH_PROG(XML2_CONFIG, xml2-config, no)
if test "x$XML2_CONFIG" = "xno" ; then
   AC_MSG_ERROR([xml2-config not found, please install libxml2, available at http://www.xmlsoft.org/.], [1])
fi

LIBXML2_LDFLAGS="`xml2-config --libs`"
LIBXML2_CFLAGS="`xml2-config --cflags`"

AC_SUBST([LIBXML2_LDFLAGS])
AC_SUBST([LIBXML2_CFLAGS])

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
support_gnome=yes
AC_ARG_ENABLE(gnome,
  [  --disable-gnome	  disable build of gnome-console GUI (enabled)],
  [if test x$enableval = xno; then
    support_gnome=no
  fi])

GDDCCONTROL=
if test x$support_gnome = xyes; then
   AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
   if test "x$PKG_CONFIG" = "xno" ; then
      AC_MSG_ERROR(pkg-config not found, please install pkg-config)
   fi

   echo -n "checking for gtk+>=2.4 and gthread>=2.4... "
   if pkg-config --atleast-version=2.4 gtk+-2.0 gthread-2.0 ; then
      GNOME_LDFLAGS="$LIBXML2_LDFLAGS `pkg-config --libs gtk+-2.0 gthread-2.0`"
      GNOME_CFLAGS="$LIBXML2_CFLAGS `pkg-config --cflags gtk+-2.0 gthread-2.0`"
      GDDCCONTROL=gddccontrol
      echo "yes"
      
      # Check for X11
      AC_PATH_XTRA
   
      AC_CHECK_LIB(Xinerama, XineramaQueryExtension,
        AC_DEFINE(HAVE_XINERAMA, 1, [Xinerama support]) XINERAMA_LIBS=-lXinerama, 
      ,$X_LIBS)
      AC_SUBST([XINERAMA_LIBS])
   else
      echo "no"
      AC_MSG_ERROR([gtk+>=2.4 development packages not found])
   fi
fi

AC_SUBST([GNOME_LDFLAGS])
AC_SUBST([GNOME_CFLAGS])
AC_SUBST([GDDCCONTROL])

# Doc check
support_doc=no
AC_ARG_ENABLE(doc,
  [  --enable-doc	  enable build of the documentation (disabled)],
  [if test x$enableval = xyes; then
    support_doc=yes
  fi])

AM_CONDITIONAL(BUILD_DOC, test x$support_doc = xyes)

DOC=
if test x$support_doc = xyes; then
   # Docbook tests
   # Based on http://www.movement.uklinux.net/docs/docbook-autotools/configure.html
   # Copyright © 2003 John Levon
   # It's just rude to go over the net to build
   XSLTPROC_FLAGS=--nonet

   AC_MSG_CHECKING([whether /etc/xml/catalog exists])
   if test ! -f /etc/xml/catalog; then
      AC_MSG_ERROR([/etc/xml/catalog does not exists, fix this problem or disable doc building])
   else
      AC_MSG_RESULT(yes)
      AC_PATH_PROG(XSLTPROC,xsltproc,no)
      if test "x$XSLTPROC" = "xno" ; then
         AC_MSG_ERROR([xsltproc does not exists, install it or disable doc building])
      fi

      AC_PATH_PROG(SSH,ssh,ssh)      
      AC_PATH_PROG(SCP,scp,scp)      
      AC_PATH_PROG(ASPELL,aspell,no)
      AC_PATH_PROG(FOP,fop,no)

      AC_PATH_PROG(TIDY,tidy,no)
      if test "x$TIDY" = "xno" ; then
         AC_MSG_ERROR([tidy does not exists, install it or disable doc building])
      fi
                  
      AC_MSG_CHECKING([whether xsltproc works])

      DOCBOOK_ROOT="http://docbook.sourceforge.net/release/xsl/current/xhtml"
      DB_FILE="$DOCBOOK_ROOT/docbook.xsl"

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

AC_SUBST(XML_CATALOG)
AC_SUBST(XSLTPROC_FLAGS)
AC_SUBST(DOCBOOK_ROOT)
AC_SUBST(DOC)

AC_CONFIG_FILES([ 
	po/Makefile.in
	Makefile
	src/Makefile
	src/lib/Makefile 
	src/ddccontrol/Makefile
	src/gddccontrol/Makefile
	src/ddcpci/Makefile
	doc/Makefile])
AC_OUTPUT
