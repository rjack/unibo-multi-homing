#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([multi-homing], [0607], [ritucci@cs.unibo.it])
AC_CONFIG_AUX_DIR([build-aux])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([src/psend.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

AC_SUBST(CFLAGS, ["-O3 -fomit-frame-pointer"])
#AC_SUBST(CFLAGS, ["-g"])
AC_SUBST(CPPFLAGS, ["-Wall -Werror -Wno-unused-function -ansi -pedantic -DNDEBUG"])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

AC_MSG_CHECKING(for MSG_NOSIGNAL)
AC_TRY_COMPILE([#include <sys/socket.h>], [
	int f;
	f = MSG_NOSIGNAL;
], [
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_MSG_NOSIGNAL, 1, [use MSG_NOSIGNAL for send()])
], [
	AC_MSG_RESULT(no)
])

case "${target_os}" in
"")
	SYS=unknown
	;;
linux*)
	SYS=linux
	;;
*bsd*)
	SYS=bsd
	;;
darwin*)
	SYS=darwin
	;;
*)
	SYS="${target_os}"
	;;
esac

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([floor gettimeofday memset select socket strchr strerror strtol])

if test "${SYS}" = "linux"; then
	AC_DEFINE(LINUX_OS, 1, Define if we compile for a Linux system)
elif test "${SYS}" = "bsd"; then
	AC_DEFINE(BSD_OS, 1, Define if we compile for a BSD system)
elif test "${SYS}" = "darwin"; then
	AC_DEFINE(DARWIN_OS, 1, Define if we compile for a Darwin system)
fi

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

printf "
Configurazione:
---------------
sistema: ${SYS}

Per compilare digitare 'make' e incrociare le dita.
Buona fortuna!
"
