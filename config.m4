PHP_ARG_ENABLE([pdo_ords], [whether to enable the PDO ORDS extension], [AS_HELP_STRING([--enable-pdo-ords], [Enable PDO ORDS extension])], [yes])

if test "$PHP_PDO_ORDS" != "no"; then
  if test -f "$HOME/.cargo/env"; then
    . "$HOME/.cargo/env"
  fi
  AC_PATH_PROG([CARGO], [cargo], [no])
  if test "$CARGO" = "no"; then
    AC_MSG_ERROR([Rust Cargo is required to build pdo_ords])
  fi
  AC_MSG_CHECKING([for Rust toolchain and PHP headers])
  PHP_CONFIG="${PHP_CONFIG:-php-config}" "$CARGO" build --release --lib
  if test "$?" != "0"; then
    AC_MSG_ERROR([Cargo failed to build the PDO ORDS backend])
  fi
  AC_MSG_RESULT([found])
  PDO_ORDS_SHARED_LIBADD="$abs_srcdir/target/release/libphp_pdo_ords.a -ldl -lpthread -lm"
  PHP_SUBST([PDO_ORDS_SHARED_LIBADD])
  PHP_NEW_EXTENSION([pdo_ords], [c/pdo_ords_loader.c], [$ext_shared])
fi
