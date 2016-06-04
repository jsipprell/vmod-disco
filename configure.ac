AC_PREREQ(2.59)
AC_COPYRIGHT([Your mom])
AC_INIT([libvmod_disco], [0.0.1])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build])
m4_ifndef([VARNISH_VMOD_INCLUDES], AC_MSG_ERROR([Needs varnish m4 -- is MI-varnish41-devel installed?]))
AM_CONFIG_HEADER(include/config.h)

AC_CANONICAL_SYSTEM
AC_LANG(C)

AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])

AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_CC_STDC
if test "x$ac_cv_prog_cc_c99" = xno; then
  AC_MSG_ERROR([Could not find a C99 compatible compiler])
fi
AC_PROG_CPP

AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

AC_ARG_WITH([libadns],
  AS_HELP_STRING([--with-libadns=PATH], [Location of libadns (auto)]),
  [dnl
    found_incl=
    found_libdir=
    for d in `echo "${withval}/include:${withval}" | tr : " "`; do
      if test -d "$d"; then
        found_incl="$d"
        break
      fi
    done
    test x"$found_incl" != x"" || found_incl="$withval"
    for d in `echo "${withval}/lib64:${withval}/lib:${withval}" | tr : " "`; do
      if test -d "$d"; then
        found_libdir="$d"
        break
      fi
    done
    LIBADNS_INCLUDES="$found_incl"
    AS_VAR_IF(found_libdir,[""],[],[AX_APPEND_FLAG(-L$found_libdir, [LDFLAGS])])
  ],[LIBADNS_INCLUDES='auto'])

AC_CHECK_PROGS(RST2MAN, [rst2man rst2man.py], "no")
if test x"$RST2MAN" = x"no"; then
  AC_MSG_WARN([rst2man not found - not building man pages])
fi
AM_CONDITIONAL(HAVE_RST2MAN, [test x"$RST2MAN" != x"no"])


AC_HEADER_STDC
AC_CHECK_HEADERS([sys/stdlib.h])

m4_ifndef([PKG_CHECK_VAR], [
AC_DEFUN([PKG_CHECK_VAR],
  [AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
  AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl
  _PKG_CONFIG([$1], [variable="][$3]["], [$2])
  AS_VAR_COPY([$1], [pkg_cv_][$1])
  AS_VAR_IF([$1], [""], [$5], [$4])dnl
  ])# PKG_CHECK_VAR
])

PKG_CHECK_MODULES([libvarnishapi], [varnishapi])
PKG_CHECK_VAR([LIBVARNISHAPI_DATAROOTDIR], [varnishapi], [datarootdir])
PKG_CHECK_VAR([LIBVARNISHAPI_BINDIR], [varnishapi], [bindir])
PKG_CHECK_VAR([LIBVARNISHAPI_SBINDIR], [varnishapi], [sbindir])
AC_SUBST([LIBVARNISHAPI_DATAROOTDIR])

save_CPPFLAGS="$CPPFLAGS"
save_LIBS="$LIBS"

AS_VAR_IF(LIBADNS_INCLUDES, ["auto"], [], [dnl
  CPPFLAGS="${CPPFLAGS} -I${LIBADNS_INCLUDES}"
  ADNS_CPPFLAGS="-I${LIBADNS_INCLUDES}"
  AX_APPEND_FLAG($ADNS_CPPFLAGS, [CPPFLAGS])
])

ADNS_LIBS=
AX_HAVE_ADNS([dnl
    AC_DEFINE([HAVE_LIBADNS], [1], [Define if we have libadns])
    ADNS_LIBS='-ladns'
    AX_APPEND_FLAG(-ladns, [LIBS])
  ], [dnl
    AC_MSG_ERROR([Cannot find libadns])
  ])

AC_SUBST([ADNS_CPPFLAGS])
AC_SUBST([ADNS_LIBS])

CPPFLAGS="$save_CPPFLAGS"

LIBS=""
AC_CHECK_LIB(rt, clock_gettime)
RT_LIBS="$LIBS"
AS_VAR_IF(LIBS, [""], [LIBS="$save_LIBS"], [LIBS="$save_LIBS $RT_LIBS"])

AC_CHECK_FUNCS([clock_gettime])
AC_SUBST(RT_LIBS)

AC_ARG_VAR([ADDL_DEFS], [Additional or custom preprocessor defines])

VARNISH_VMOD_INCLUDES
VARNISH_VMOD_DIR
VARNISH_VMODTOOL

AC_PATH_PROG([VARNISHTEST], [varnishtest], [],
  [$LIBVARNISHAPI_BINDIR:$LIBVARNISHAPI_SBINDIR:$PATH])
AC_PATH_PROG([VARNISHD], [varnishd], [],
  [$LIBVARNISHAPI_SBINDIR:$LIBVARNISHAPI_BINDIR:$PATH])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
])

AC_OUTPUT
