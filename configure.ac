#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(wm, [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/screen.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lxcb':
AC_CHECK_LIB([xcb], [main])
AC_CHECK_LIB([xcb-xinput], [main])
AC_CHECK_LIB([xcb-util], [main])

AC_CHECK_LIB([suinput], [main])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT32_T

# Checks for library functions.

AC_CONFIG_FILES([src/Makefile])
AC_OUTPUT(Makefile)
