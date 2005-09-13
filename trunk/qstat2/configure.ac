AC_INIT([qstat],[2.8.1],[qstat-users@yahoogroups.com])
AC_CONFIG_SRCDIR([qstat.c])
AM_CONFIG_HEADER([gnuconfig.h])

AC_PREREQ(2.53)
AM_INIT_AUTOMAKE([1.6 foreign])

dnl Checks for programs.
AC_PROG_CC

dnl Checks for header files.
AC_HEADER_STDC

dnl check if user wants debug
AC_ARG_ENABLE(debug,[  --disable-debug         turn off debugging code])
if test x$enable_debug != xno; then
	CPPFLAGS="$CPPFLAGS -DDEBUG"
fi

AC_ARG_ENABLE(dump,[  --enable-dump           enable packet dumps])
if test x$enable_dump != xno; then
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
