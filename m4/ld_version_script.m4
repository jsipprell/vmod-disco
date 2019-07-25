AC_DEFUN([LD_VERSION_SCRIPT],
[
  AC_ARG_ENABLE([ld-version-script],
    AS_HELP_STRING([--enable-ld-version-script],
      [enable linker version script (default enabled when possible)]),
      [have_ld_version_script=$enableval], [])

  AS_VAR_IF(have_ld_version_script,[""],
    [AC_MSG_CHECKING([if ld -Wl,--version-script works])
     save_LDFLAGS="$LDFLAGS"
     LDFLAGS="$LDFLAGS -Wl,--version-script=conftest.map"
     cat > conftest.map <<EOF
VERS_1 {
  global: sym;
};
VERS_2 {
  global: sym;
} VERS_1;
EOF

    AC_LANG_PUSH(C)
    AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                   [have_ld_version_script=yes],[have_ld_version_script=no])
    AC_LANG_POP
    rm -f conftest.map
    LDFLAGS="$save_LDFLAGS"
    AC_MSG_RESULT($have_ld_version_script)],
  [])
  AS_VAR_IF(have_ld_version_script,["yes"],
    [LDFLAGS_VERSION='-Wl,--version-script=$1'],
    [LDFLAGS_VERSION=])
  AC_SUBST([LDFLAGS_VERSION])
])

