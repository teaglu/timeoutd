
AC_PREREQ([2.69])
AC_INIT([timeoutd], [0.9], [info@teaglu.com])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_LANG_PUSH(C++)

AC_TYPE_SIZE_T

ACCEPT_SSL_LIB="no"
AC_CHECK_LIB(ssl, OPENSSL_init_ssl, [ACCEPT_SSL_LIB="yes"])
AC_CHECK_LIB(ssl, SSL_library_init, [ACCEPT_SSL_LIB="yes"])
AS_IF([test "x$ACCEPT_SSL_LIB" = xno],
	[AC_MSG_ERROR([library 'ssl' is required for OpenSSL])])

AC_OUTPUT(Makefile src/Makefile)

