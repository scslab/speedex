AC_INIT([speedex], [0])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

dnl note to future geoff: -flto seems to break lmdb, so don't use it -- 12/2/2021 this is wrong, seems to have been a result of accidental stack use-after-scope
OPT_FLAGS="-march=native -O3 -flto"
DEBUG_FLAGS="-ggdb -O0"

CXXFLAGS="-std=gnu++2a -fcoroutines $DEBUG_FLAGS $CXXFLAGS"
CXX="g++"

AC_PROG_CXX
AC_LANG(C++)
dnl AX_CXX_COMPILE_STDCXX(17,,mandatory)
AX_APPEND_COMPILE_FLAGS([-pthread])
AS_IF([test -z "${WFLAGS+set}"], AX_APPEND_COMPILE_FLAGS([-Wall], WFLAGS))

CXXFLAGS="$CXXFLAGS $WFLAGS"

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([gsl], [gsl])
PKG_CHECK_MODULES([libsodium], [libsodium])
PKG_CHECK_MODULES([lmdb], [lmdb])
PKG_CHECK_MODULES([tbb], [tbb])
PKG_CHECK_MODULES([Catch2], [Catch2])
PKG_CHECK_MODULES(LEMON, [lemon], HAVE_LEMON=1, HAVE_LEMON=0)

AC_DEFINE_UNQUOTED([HAVE_LEMON], [$HAVE_LEMON], [Define to 1 if you have lemon available])

PKG_CHECK_MODULES(LIBFYAML, [ libfyaml ], HAVE_LIBFYAML=1, HAVE_LIBFYAML=0)

if test "x$HAVE_LIBFYAML" != "x1" ; then
	AC_MSG_ERROR([failed to find libfyaml])
fi

AC_SUBST(HAVE_LIBFYAML)
AC_SUBST(LIBFYAML_CFLAGS)
AC_SUBST(LIBFYAML_LIBS)
AC_DEFINE_UNQUOTED([HAVE_LIBFYAML], [$HAVE_LIBFYAML], [Define to 1 if you have libfyaml available])
AM_CONDITIONAL([HAVE_LIBFYAML], [ test x$HAVE_LIBFYAML = x1 ])

AS_IF([test -z "${ROOT_DB_DIRECTORY}"], [ROOT_DB_DIRECTORY="databases/"])
AS_IF([test -z "${ACCOUNT_DB}"], [ACCOUNT_DB="account_database/"])
AS_IF([test -z "${OFFER_DB}"], [OFFER_DB="offer_database/"])
AS_IF([test -z "${HEADER_HASH_DB}"], [HEADER_HASH_DB="header_hash_database/"])
AS_IF([test -z "${TX_BLOCK_DB}"], [TX_BLOCK_DB="tx_block_database/"])
AS_IF([test -z "${HEADER_DB}"], [HEADER_DB="header_database/"])
AS_IF([test -z "${HOTSTUFF_BLOCKS}"], [HOTSTUFF_BLOCKS="hotstuff_blocks/"])
AS_IF([test -z "${HOTSTUFF_INDEX}"], [HOTSTUFF_INDEX="hotstuff_index/"])

AC_DEFINE_UNQUOTED([ROOT_DB_DIRECTORY], ["$ROOT_DB_DIRECTORY"], [Root directory for storing lmdbs + txs])
AC_DEFINE_UNQUOTED([ACCOUNT_DB], ["$ACCOUNT_DB"], [Subdirectory of ROOT_DB_DIRECTORY for account lmdb])
AC_DEFINE_UNQUOTED([OFFER_DB], ["$OFFER_DB"], [Subdirectory of ROOT_DB_DIRECTORY for workunit lmdbs])
AC_DEFINE_UNQUOTED([HEADER_HASH_DB], ["$HEADER_HASH_DB"], [Subdirectory of ROOT_DB_DIRECTORY for header hash lmdb])
AC_DEFINE_UNQUOTED([TX_BLOCK_DB], ["$TX_BLOCK_DB"], [Subdirectory of ROOT_DB_DIRECTORY for transaction block lists])
AC_DEFINE_UNQUOTED([HEADER_DB], ["$HEADER_DB"], [Subdirectory of ROOT_DB_DIRECTORY for block headers])
AC_DEFINE_UNQUOTED([HOTSTUFF_BLOCKS], ["$HOTSTUFF_BLOCKS"], [Subdirectory of ROOT_DB_DIRECTORY for hotstuff blocks])
AC_DEFINE_UNQUOTED([HOTSTUFF_INDEX], ["$HOTSTUFF_INDEX"], [Subdirectory of ROOT_DB_DIRECTORY for hotstuff block index])

AS_MKDIR_P([$ROOT_DB_DIRECTORY$ACCOUNT_DB])
AS_MKDIR_P([$ROOT_DB_DIRECTORY$OFFER_DB])
AS_MKDIR_P([$ROOT_DB_DIRECTORY$HEADER_HASH_DB])
AS_MKDIR_P([$ROOT_DB_DIRECTORY$TX_BLOCK_DB])
AS_MKDIR_P([$ROOT_DB_DIRECTORY$HEADER_DB])
AS_MKDIR_P([$ROOT_DB_DIRECTORY$HOTSTUFF_BLOCKS])
AS_MKDIR_P([$ROOT_DB_DIRECTORY$HOTSTUFF_INDEX])

AS_MKDIR_P(["experiment_data/"])
AS_MKDIR_P(["experiment_results/"])

export CXXFLAGS

dnl AC_CONFIG_SUBDIRS([xdrpp])

AX_PKGCONFIG_SUBDIR(xdrpp)
AC_MSG_CHECKING(for xdrc)
AS_IF([test -n "$XDRC"],,
	    [test -n "$xdrpp_INTERNAL" -a x"$cross_compiling" != xyes],
	    [XDRC='$(top_builddir)/xdrpp/xdrc/xdrc$(EXEEXT)'],
	    [AC_PATH_PROG(XDRC, [xdrc])])
if test -z "$XDRC"; then
   AC_MSG_ERROR(Cannot find xdrc)
fi
AC_MSG_RESULT($XDRC)
AC_SUBST(XDRC)

ABS_TOP_DIR=$ac_pwd
export ABS_TOP_DIR

AX_PKGCONFIG_SUBDIR(lib/hotstuff)

export xdrpp_INTERNAL
global_xdrpp_CFLAGS=$xdrpp_CFLAGS
global_xdrpp_LIBS=$xdrpp_LIBS
global_XDRC=$XDRC
export global_xdrpp_CFLAGS
export global_xdrpp_LIBS
export global_XDRC

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
