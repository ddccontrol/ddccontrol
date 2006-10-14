AC_INIT([DDC/CI control tool database], [20061014],
        [DDCControl users list <ddccontrol-users@lists.sourceforge.net>],
        [ddccontrol-db])
AM_INIT_AUTOMAKE
AC_PROG_INSTALL

AM_MAINTAINER_MODE

AM_GNU_GETTEXT([external])
GETTEXT_PACKAGE=ddccontrol-db
AC_SUBST([GETTEXT_PACKAGE])

AC_CHECK_PROG(PERL,perl,perl,)

AM_CONDITIONAL(HAVE_PERL, test -n $PERL)

AC_CONFIG_FILES([
	po/Makefile.in
	Makefile
	db/Makefile])

AC_PROG_INTLTOOL

AC_OUTPUT
