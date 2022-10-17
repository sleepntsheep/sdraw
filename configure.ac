AC_INIT([sdraw], [0.1], [contact@papangkorn.com])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_PROG_CC
PKG_CHECK_MODULES(
  [SDL2],
  [sdl2 > 2.0.0],
  [CFLAGS="$CFLAGS $SDL2_CFLAGS"
   LIBS="$SDL2_LIBS $LIBS"],
  AC_MSG_ERROR($SDL2_PKG_ERRORS)
)
AC_CHECK_LIB(SDL2, SDL_Quit)
AC_CHECK_LIB(SDL2_ttf, TTF_Quit)
AC_CHECK_LIB(fontconfig, Fc_Init)
AC_CHECK_LIB(m, acos)
AC_CHECK_FUNCS([floor])
AC_CHECK_FUNCS([acos])
AC_CHECK_FUNCS([strtol])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
