AC_INIT([DDC/CI control tool], [0.1],
        [Nicolas Boichat <nicolas@boichat.ch>],
        [ddccontrol])
AM_CONFIG_HEADER(src/config.h)
AM_INIT_AUTOMAKE
AC_PROG_RANLIB
AC_PROG_CC
AC_PROG_INSTALL

AC_STDC_HEADERS
AC_CHECK_HEADERS([stdio.h unistd.h sys/time.h stdlib.h errno.h unistd.h string.h sys/types.h sys/stat.h fcntl.h sys/ioctl.h dirent.h], [], [AC_MSG_ERROR([Important header not found, please install it.], [1])], [])
AC_CHECK_HEADERS([linux/i2c-dev.h], [], [AC_MSG_ERROR([Linux kernel header files not found, please install them.], [1])], [])

# libxml2 check

AC_PATH_PROG(XML2_CONFIG, xml2-config, no)
if test "x$XML2_CONFIG" = "xno" ; then
   AC_MSG_ERROR(xml2-config not found, please install libxml2, available at http://www.xmlsoft.org/.)
fi

CFLAGS="$CFLAGS -Wall `xml2-config --cflags`"
LDFLAGS="$LDFLAGS `xml2-config --libs`"

# Gnome check

AC_ARG_ENABLE(gnome,
  [  --enable-gnome	  enable build of gnome-console GUI [disabled]],
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
      GNOME_CFLAGS="$CFLAGS `pkg-config --cflags gtk+-2.0` -DHAVE_GNOME=1"
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
	Makefile
	src/Makefile
	src/lib/Makefile 
	src/ddccontrol/Makefile
	src/gddccontrol/Makefile])
AC_OUTPUT
