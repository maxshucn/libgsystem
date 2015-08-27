AC_PREREQ([2.63])
AC_INIT([libgsystem], [2015.2], [walters@verbum.org])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([buildutil])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 -Wno-portability foreign no-define tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AC_PROG_CC
AM_PROG_CC_C_O

changequote(,)dnl
if test "x$GCC" = "xyes"; then
  WARN_CFLAGS="-Wall -Wstrict-prototypes -Werror=missing-prototypes \
		-Werror=implicit-function-declaration \
		-Werror=pointer-arith -Werror=init-self -Werror=format=2 \
		-Werror=format-security \
		-Werror=missing-include-dirs -Werror=aggregate-return \
		-Werror=declaration-after-statement"
fi
changequote([,])dnl
AC_SUBST(WARN_CFLAGS)

# Initialize libtool
LT_PREREQ([2.2.4])
LT_INIT([disable-static])

AC_CHECK_HEADER([attr/xattr.h],,[AC_MSG_ERROR([You must have attr/xattr.h from libattr])])
AC_CHECK_HEADER([sys/capability.h],,[AC_MSG_ERROR([You must have sys/capability.h from libcap])])

PKG_PROG_PKG_CONFIG

GIO_DEPENDENCY="gio-unix-2.0 >= 2.34.0"
PKG_CHECK_MODULES(BUILDDEP_GIO_UNIX, $GIO_DEPENDENCY)

GLIB_TESTS

GTK_DOC_CHECK([1.15], [--flavour no-tmpl])
AC_PATH_PROG([XSLTPROC], [xsltproc])

m4_ifdef([GOBJECT_INTROSPECTION_CHECK], [
  GOBJECT_INTROSPECTION_CHECK([1.34.0])
])
AM_CONDITIONAL(BUILDOPT_INTROSPECTION, test "x$found_introspection" = xyes)


AC_ARG_WITH(systemd-journal,
	    AS_HELP_STRING([--without-systemd-journal], [Use systemd journal @<:@default=auto@:>@]),
	    [], [with_systemd_journal=auto])
AS_IF([test x$with_systemd_journal != xno], [
  PKG_CHECK_MODULES([BUILDDEP_SYSTEMD_JOURNAL], [libsystemd-journal >= 200], have_systemd_journal=yes, have_systemd_journal=no)
  ])
AM_CONDITIONAL(ENABLE_SYSTEMD_JOURNAL, test x$have_systemd_journal = xyes)
AS_IF([test x$have_systemd_journal = xyes], [
  AC_DEFINE([ENABLE_SYSTEMD_JOURNAL],[1],[Define if you want to build with systemd journal support])
])

AC_CONFIG_FILES([
Makefile
src/libgsystem.pc
])
AC_OUTPUT

echo "
    libgsystem $VERSION
    ====================

    introspection:                                $found_introspection
    systemd journal:                              $have_systemd_journal
"
