AC_INIT([qstat],[2.7.1],[qstat-users@yahoogroups.com])
AC_CONFIG_SRCDIR([qstat.c])
AM_CONFIG_HEADER([gnuconfig.h])

AM_INIT_AUTOMAKE([foreign])

dnl Checks for programs.
AC_PROG_CC

dnl Checks for header files.
AC_HEADER_STDC

dnl check if user wants debug
AC_ARG_ENABLE(debug,[  --enable-debug          turn on debugging ])
if test x$enable_debug = xyes; then
	CPPFLAGS="$CPPFLAGS -DDEBUG"
fi

AC_ARG_ENABLE(dump,[  --enable-dump           enable package dumps])
if test x$enable_dump = xyes; then
	CPPFLAGS="$CPPFLAGS -DENABLE_DUMP"
fi

dnl Use -Wall if we have gcc.
changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
fi
changequote([,])dnl

AC_CONFIG_FILES([
	Makefile
	template/Makefile
	info/Makefile
])
AC_OUTPUT
