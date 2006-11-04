AC_INIT([qstat],[2.11],[qstat-users@yahoogroups.com])
AC_CONFIG_SRCDIR([qstat.c])
AM_CONFIG_HEADER([gnuconfig.h])

AC_PREREQ(2.53)
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.6 foreign])

dnl Checks for programs.
AC_PROG_CC

dnl Checks for header files.
AC_HEADER_STDC

AC_CHECK_HEADER(sys/mman.h, [have_mman_h=yes])

case $host in
	*mingw32*)
		AC_MSG_NOTICE([compiling for $host, adding -lwsock32])
		LIBS="$LIBS -lwsock32"
	;;
esac

dnl check if user wants debug
AC_MSG_CHECKING([whether to enable debug output])
AC_ARG_ENABLE(debug,[  --disable-debug         turn off debugging code])
if test x$enable_debug != xno; then
	CPPFLAGS="$CPPFLAGS -DDEBUG"
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([whether to enable packet dumps])
AC_ARG_ENABLE(dump,[  --enable-dump           enable packet dumps])
if test x$enable_dump != xno; then
	if test x$have_mman_h = xyes; then
		CPPFLAGS="$CPPFLAGS -DENABLE_DUMP"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no, sys/mman.h missing])
	fi
else
	AC_MSG_RESULT([no])
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
