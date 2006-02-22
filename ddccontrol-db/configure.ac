AC_INIT([DDC/CI control tool database], [20060222],
        [DDCControl users list <ddccontrol-users@lists.sourceforge.net>],
        [ddccontrol-db])
AM_INIT_AUTOMAKE
AC_PROG_INSTALL

AM_MAINTAINER_MODE

AM_PO_SUBDIRS

AC_CHECK_PROG(PERL,perl,perl,)

AM_CONDITIONAL(HAVE_PERL, test -n $PERL)

AC_CONFIG_FILES([
	po/Makefile.in
	Makefile
	db/Makefile])
AC_OUTPUT
