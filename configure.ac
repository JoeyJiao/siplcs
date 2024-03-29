dnl ensure recent enough version of Autoconf
AC_PREREQ([2.50])

dnl Use `VERSION' files to get version.
m4_define([SIPE_VERSION_DATA], m4_include([VERSION]))

dnl Strip off newline characters.
m4_define([SIPE_VERSION],
          m4_substr(SIPE_VERSION_DATA,
                    [0], m4_decr(m4_len(SIPE_VERSION_DATA))))

dnl homepage
m4_define([SIPE_HOMEPAGE], [http://sipe.sourceforge.net/])

dnl initialize package name, version, bugtracker & homepage
AC_INIT([pidgin-sipe], SIPE_VERSION,
	[https://sourceforge.net/tracker/?atid=949931&group_id=194563], [],
	SIPE_HOMEPAGE)

AC_DEFINE([SIPE_TRANSLATIONS_URL],
	["https://www.transifex.net/projects/p/pidgin-sipe/r/mob/"],
	[URL for submitting translations.])

dnl setup automake and require recent enough version
AM_INIT_AUTOMAKE([1.9 dist-bzip2 no-define tar-ustar])

AC_CANONICAL_HOST

dnl set PACKAGE_URL for autoconf < 2.64
AS_IF([test "x${PACKAGE_URL}" = x],
	[AC_MSG_NOTICE([set PACKAGE_URL for autoconf < 2.64])
	 AC_DEFINE(PACKAGE_URL, "SIPE_HOMEPAGE", [Define to the home page for this package.])
])

dnl set programming language
AC_LANG(C)
AM_PROG_CC_C_O
AC_HEADER_STDC

dnl setup shared library generation
LT_INIT([disable-static])

dnl checks for tools
IT_PROG_INTLTOOL([0.35.0])
AC_PROG_INSTALL
AC_PROG_RANLIB

dnl ******************************
dnl Win32
dnl ******************************
AC_MSG_CHECKING([for Win32])
AS_CASE(["$host"],
		[*-mingw*], [os_win32=yes],
		[os_win32=no])
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(SIPE_OS_WIN32, [test "x${os_win32}" = xyes])

dnl checks for header files
AC_CHECK_HEADERS([sys/sockio.h])

dnl checks for library functions
AC_CHECK_FUNCS([])

dnl tell pkgconfig to look in the same prefix where we're installing this to,
dnl as that is likely where libpurple will be found if it is not in the default
dnl pkgconfig path
PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${libdir}/pkgconfig"
export PKG_CONFIG_PATH

dnl debug mode
AC_ARG_ENABLE(debug,
	[AS_HELP_STRING([--enable-debug],
			[compile with debugging support [default=no]])],
	[],
	[enable_debug=no])

AS_IF([test "x$enable_debug" = xyes],
	[AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])])
AC_SUBST(DEBUG_CFLAGS)

dnl quality check mode
dnl For people who are interested in finding bugs and not hiding them
AC_ARG_ENABLE(quality-check,
	[AS_HELP_STRING([--enable-quality-check],
			[compile with compiler checks enabled [default=yes]])],
	[],
	[enable_quality_check=yes])

AS_IF([test "x$enable_quality_check" = xyes],
	[dnl default quality configuration
         QUALITY_CFLAGS="$QUALITY_CFLAGS -Werror -Wall"

	 dnl enable warnings supported by the GCC on the build system
	 for newflag in \
			"-Wextra" \
			"-Waggregate-return" \
			"-Wcast-align" \
			"-Wdeclaration-after-statement" \
			"-Wdeprecated-declarations" \
			"-Winit-self" \
			"-Wmaybe-uninitialized" \
			"-Wmissing-declarations" \
			"-Wmissing-prototypes" \
			"-Wnested-externs" \
			"-Wpointer-arith" \
			"-Wundef" \
			"-Wunused-but-set-variable" \
	 ; do
		 ac_save_CFLAGS=$CFLAGS
		 AC_MSG_CHECKING([if $CC supports $newflag])
		 CFLAGS="$QUALITY_CFLAGS $newflag"
		 AC_COMPILE_IFELSE(
			[AC_LANG_SOURCE([[]])],
			[AC_MSG_RESULT(yes)
			 QUALITY_CFLAGS="$QUALITY_CFLAGS $newflag"],
			[AC_MSG_RESULT(no)]
	 	 )
		 CFLAGS=$ac_save_CFLAGS
	 done
	])
AC_SUBST(QUALITY_CFLAGS)

dnl Check for pkg-config before using it
PKG_PROG_PKG_CONFIG

dnl check for valgrind (optional, only needed for debugging)
PKG_CHECK_MODULES(VALGRIND, [valgrind],
	[AC_DEFINE(HAVE_VALGRIND, 1, [Define to 1 if you have the valgrind headers])],
	[AC_MSG_RESULT(no)])

dnl build option: with voice & video support (for all backends)
AC_ARG_WITH(vv,
	[AC_HELP_STRING([--with-vv],
		[compile with voice and video support @<:@default=check@:>@])],
	[AS_IF([test "x$withval" = xyes], [with_vv=check])],
	[with_vv=check])

dnl check for Kerberos 5 support
AC_ARG_WITH([krb5],
	[AC_HELP_STRING([--with-krb5],
		[compile with Kerberos 5 support @<:@default=check@:>@])],
	[AS_IF([test "x$withval" = xyes], [with_krb5=check])],
	[with_krb5=check])

AS_IF([test "x$with_krb5" = xno], [], dnl disabled by user
      [test "x$with_krb5" = xcheck],  dnl autodetect with krb5-config
	[AC_MSG_CHECKING(if Kerberos 5 is available)
	 AS_IF([AC_RUN_LOG([krb5-config --version])],
		[KRB5_CFLAGS=`krb5-config --cflags 2>/dev/null`
		 KRB5_LDFLAGS=""

		 dnl same as AC_CHECK_LIB()
		 LIBS="$LIBS `krb5-config --libs gssapi 2>/dev/null`"
		 AC_DEFINE(HAVE_LIBKRB5, 1, [Define to 1 if you have the `krb5' library (-lkrb5)])],
		[AC_MSG_RESULT(no)
		 with_krb5=no])],
      [                               dnl path specified by user
	KRB5_CFLAGS="-I${with_krb5}/include"
	KRB5_LDFLAGS="-L${with_krb5}/lib"
	ac_save_LDFLAGS="$LDLAGS"
	LDFLAGS="$LDFLAGS $KRB5_LDFLAGS"
	AC_CHECK_LIB(krb5, krb5_rd_req,          [],
		[AC_CHECK_LIB(krb5, krb5_rd_req, [],
			[AC_ERROR(Kerberos 5 libraries not found)])])
	AC_CHECK_LIB(gssapi_krb5, gss_krb5_export_lucid_sec_context, [],
			[AC_ERROR(GSSAPI Kerberos 5 libraries not found)])
	LDFLAGS="$ac_save_LDFLAGS"])
AM_CONDITIONAL(SIP_SEC_KRB5, [test "x$with_krb5" != xno])
AC_SUBST(KRB5_CFLAGS)
AC_SUBST(KRB5_LDFLAGS)

dnl check for glib
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.12.0])
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= 2.12.0])

dnl check for gmime
dnl See also: https://bugzilla.gnome.org/show_bug.cgi?id=613653#c8
PKG_CHECK_MODULES(GMIME, [gmime-2.6 >= 2.5.2],
	[ac_have_gmime=yes],
	[PKG_CHECK_MODULES(GMIME, [gmime-2.4 >= 2.4.16],
		[ac_have_gmime=yes],
		[ac_have_gmime=no])
])
AM_CONDITIONAL(SIPE_MIME_GMIME, [test "x$ac_have_gmime" = xyes])
AS_IF([test "x$ac_have_gmime" = xyes],
	[AC_DEFINE(HAVE_GMIME, 1, [Define if gmime should be used in sipe.])])

dnl check for NSS
PKG_CHECK_MODULES(NSS, [nss],
	[],
	[PKG_CHECK_MODULES(NSS, [mozilla-nss],
		[],
		[PKG_CHECK_MODULES(NSS, [microb-engine-nss],
			[],
			[AC_ERROR([nss, mozilla-nss or microb-engine-nss package is required])])
	])
])

dnl check for libxml2
PKG_CHECK_MODULES(LIBXML2, [libxml-2.0])

dnl assumption check: sizof(uuid_t) must be 16 (see uuid.c)
AC_MSG_CHECKING([that sizeof(uuid_t) is 16])
ac_save_CFLAGS=$CFLAGS
CFLAGS="$GLIB_CFLAGS $QUALITY_CFLAGS"
dnl note the [[[ quoting: our code contains []!
AC_RUN_IFELSE(
	[AC_LANG_PROGRAM(
		[[
#include <glib.h>
#include <stdio.h>

/* Copied from uuid.c */
typedef struct {
   guint32 time_low;
   guint16 time_mid;
   guint16 time_hi_and_version;
   guint8  clock_seq_hi_and_reserved;
   guint8  clock_seq_low;
   guint8  node[6];
} uuid_t;
		]],
		[[if (sizeof(uuid_t) == 16) {
			return(0);
		 } else {
			printf("\n\nOoops, sizeof(uuid_t) is %" G_GSIZE_FORMAT ".\n\n", sizeof(uuid_t));
			return(1);
		 }]])],
	[AC_MSG_RESULT(yes)],
	[AC_MSG_FAILURE([sizeof(uuid_t) is not 16

Please notify the SIPE developers.

Copy & paste all lines starting with the line

  checking that sizeof(uuid_t) is 16...

to the report and attach the file "config.log".

Compiler information:

 host:     $host
 build:    $build
 compiler: $CC
])],
	[AC_MSG_WARN([cross compiling: not checking])])
CFLAGS=$ac_save_CFLAGS

dnl build option: OCS2005 client hack
AC_ARG_ENABLE([ocs2005-message-hack],
	[AC_HELP_STRING([--enable-ocs2005-message-hack], [disable message timeout for OCS2005 clients
							  which causes "false" not delivered error messages
							  @<:@default=no@:>@])],
	[AC_DEFINE([ENABLE_OCS2005_MESSAGE_HACK], [1],
	           [Define to 1 to disable SIP MESSAGE timeout feature.
		    OCS2005 clients don't seem to acknowledge MESSAGEs and
		    disabling the timeout suppresses "false" error messages])])

dnl build option: purple backend
AC_ARG_ENABLE([purple],
	[AC_HELP_STRING([--enable-purple], [build purple plugin @<:@default=yes@:>@])],
	[],
	[enable_purple=yes])
with_purple_vv=no
AS_IF([test "x$enable_purple" != xno],
	[PKG_CHECK_MODULES(PURPLE, [purple >= 2.4.0],
		[dnl unfortunately PURPLE_MAJOR_VERSION is not exported by purple.m4
		 PURPLE_MAJOR_VERSION=`$PKG_CONFIG --modversion purple | cut -d. -f 1`
		 AC_SUBST(PURPLE_MAJOR_VERSION)

		 dnl let user now if he has a 32- and 64-bit header conflict...
		 AC_MSG_CHECKING([for 32- and 64-bit header conflicts])
		 ac_save_CFLAGS=$CFLAGS
		 CFLAGS="$PURPLE_CFLAGS $QUALITY_CFLAGS"
		 AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM([[
#include <glib.h>
#include <cipher.h>

/* Compile some code that has caused trouble in the past */
static void testfunc(unsigned long len)
{
  PurpleCipherContext *rc4 = NULL;
  guchar *plaintext = NULL;
  gsize plaintext_length = 0;
  guchar *encrypted_text = NULL;
  gsize dummy;
  purple_cipher_context_set_option(rc4, "key_len", GUINT_TO_POINTER(len));
  purple_cipher_context_encrypt(rc4, plaintext, plaintext_length,
                                encrypted_text, &dummy);
}
					]],
					[[testfunc(16);]])],
			[AC_MSG_RESULT(none)],
			[AC_MSG_FAILURE([conflicts found.

Check that you have the correct glib/libpurple headers installed for
your architecture or set GLIB_CFLAGS/PURPLE_CFLAGS to override the
system defaults.
])
			])

		 dnl on windows
		 dnl test program needs to be in the same location as libpurple
		 dnl installation for latter to load ssl plugin referenced by
		 dnl RELATIVE path.
		 AS_IF([test "x${os_win32}" != xyes],
			[dnl purple SSL support is mandatory
			 AC_MSG_CHECKING([for purple SSL support])
			 ac_save_LIBS=$LIBS
			 LIBS="$PURPLE_LIBS"
			 AC_RUN_IFELSE(
				[AC_LANG_PROGRAM([[
#include <glib.h>
#include <plugin.h>
#include <sslconn.h>
						]],
						[[
	purple_plugins_init();
	purple_plugins_probe(G_MODULE_SUFFIX);
	purple_ssl_init();
	return(purple_ssl_is_supported()?0:1);
						]])],
				[AC_MSG_RESULT(ok)],
				[AC_MSG_FAILURE([your purple hasn't been compiled with SSL support.
])
				],
				[AC_MSG_WARN([cross compiling: not checking])])
		 ])

		 dnl check whether enable voice and video support
		 AS_IF([test "x$with_vv" = xno], [], dnl disabled by user
		       [test "x$with_vv" = xcheck],  dnl autodetect
			 [AC_MSG_CHECKING(for purple voice and video support)
			  AC_RUN_IFELSE(
				[AC_LANG_PROGRAM([[
#include <media.h>
					]],
					[[return (purple_media_get_type() == G_TYPE_NONE ? 1 : 0);]]
			  	)],
				[AC_MSG_RESULT(ok)
				 PKG_CHECK_MODULES(NICE, [nice >= 0.1.0],
					dnl sipe-media.c uses g_slist_free_full()
					[PKG_CHECK_MODULES(_SIPE_MEDIA_RECHECK_PLEASE_IGNORE, [purple >= 2.8.0 glib-2.0 >= 2.28.0],
						[PKG_CHECK_MODULES(GSTREAMER, [gstreamer-0.10],
							[with_purple_vv=yes],
							[AC_MSG_NOTICE(GStreamer required: disabling purple voice and video support)])],
						[AC_MSG_NOTICE(libpurple >= 2.8.0 glib2 >= 2.28.0 required: disabling purple voice and video support)])],
					[AC_MSG_NOTICE(libnice required: disabling purple voice and video support)])],
				[AC_MSG_RESULT(no - your purple hasn't been compiled with voice and video support.)],
				[AC_MSG_WARN([cross compiling: not checking])])
			 ])

		 dnl check if we have TCP media transport patches in libpurple
		 AS_IF([test "x$with_purple_vv" != xno],
			[
			 AC_MSG_CHECKING(for TCP media transport)
			 AC_COMPILE_IFELSE(
	 			[AC_LANG_PROGRAM([[#include <media.h>]],[[return PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_ACTIVE]])],
				[
				 with_purple_tcp=yes
				 AC_DEFINE(HAVE_ICE_TCP, 1, [Define if we have support for TCP media transport.])
				 AC_MSG_RESULT(yes)
				],
				[AC_MSG_RESULT(no)]
	 		 )
			])

		 CFLAGS=$ac_save_CFLAGS
		 LIBS=$ac_save_LIBS],
		[enable_purple=no])])
AM_CONDITIONAL(SIPE_INCLUDE_PURPLE, [test "x$enable_purple" != xno])

dnl build option: telepathy backend
AC_ARG_ENABLE([telepathy],
	[AC_HELP_STRING([--enable-telepathy], [build telepathy plugin @<:@default=yes@:>@])],
	[],
	[enable_telepathy=yes])
AS_IF([test "x$enable_telepathy" != xno],
	[PKG_CHECK_MODULES(TELEPATHY, [telepathy-glib],
		[dnl GMIME is a build requirement
		 AS_IF([test "x$ac_have_gmime" = xyes],
			[],
			[AC_ERROR(GMIME package is required for telepathy plugin)])

		 dnl NOTHING IMPLEMENTED YET!!!
		],
		[enable_telepathy=no])])
AM_CONDITIONAL(SIPE_INCLUDE_TELEPATHY, [test "x$enable_telepathy" != xno])

dnl sanity check
AS_IF([test "x$enable_purple" = xno -a "x$enable_telepathy" = xno],
	[AC_ERROR(at least one plugin must be selected

If you didn't use a --enable option then please check that you have
the headers for the packages "purple" or "telepathy-glib" installed.
)],
	[])

dnl enable voice & video support if any backend supports it
AS_IF([test "x$with_purple_vv" != xno],
	[AC_DEFINE(HAVE_VV, 1, [Define if voice & video is enabled.])])
AM_CONDITIONAL(SIPE_WITH_VV, [test "x$with_purple_vv" != xno])

dnl i18n
AC_MSG_CHECKING([locale_CPPFLAGS])
LOCALE_CPPFLAGS='-DLOCALEDIR=\"$(datadir)/locale\"'
AC_SUBST([LOCALE_CPPFLAGS])
AC_MSG_RESULT([$LOCALE_CPPFLAGS])
AM_GLIB_GNU_GETTEXT
AC_SUBST(GETTEXT_PACKAGE, "${PACKAGE_NAME}")

dnl codeset
AM_LANGINFO_CODESET

dnl substitutions and generated files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
	pixmaps/Makefile
	m4macros/Makefile
	po/Makefile.in
	pixmaps/16/Makefile
	pixmaps/22/Makefile
	pixmaps/48/Makefile
	pixmaps/scalable/Makefile
	src/Makefile
	src/core/Makefile
	src/api/Makefile
	src/purple/Makefile
	src/telepathy/Makefile
	])

dnl generate files
AC_OUTPUT()

dnl additional info for the user
AS_ECHO()
AS_IF([test "x$enable_purple" = xno], 
	[AS_ECHO("Not building purple plugin")],
	[AS_ECHO("Build purple plugin")
	 AS_ECHO("PURPLE_CFLAGS  : $PURPLE_CFLAGS")
	 AS_ECHO("PURPLE_LIBS    : $PURPLE_LIBS")
	 AS_ECHO_N("Voice and video: ")
	 AS_IF([test "x$with_purple_vv" = xno],
	  [AS_ECHO("disabled")],
	  [
	   AS_ECHO_N("enabled")
	   AS_IF([test "x$with_purple_tcp" = xyes], [AS_ECHO_N(" with TCP transport")])
	   AS_ECHO()
	  ])
	])
AS_ECHO()
AS_IF([test "x$enable_telepathy" = xno],
	[AS_ECHO("Not building telepathy plugin")],
	[AS_ECHO("Build telepathy plugin")
	 AS_ECHO()
	 AS_ECHO("NOTHING IMPLEMENTED YET!!!")
	 AS_ECHO()
	 AS_ECHO("TELEPATHY_CFLAGS: $TELEPATHY_CFLAGS")
	 AS_ECHO("TELEPATHY_LIBS  : $TELEPATHY_LIBS")])
AS_ECHO()
AS_IF([test "x$with_krb5" = xno],
	[AS_ECHO("Not building with Kerberos 5 support")],
	[AS_ECHO("Build with Kerberos 5 support")
	 AS_ECHO("KRB5_CFLAGS    : $KRB5_CFLAGS")
	 AS_ECHO("KRB5_LDFLAGS   : $KRB5_LDFLAGS")])
AS_ECHO()
AS_IF([test "x$enable_debug" = xno],
	[AS_ECHO("Debugging not enabled")],
	[AS_ECHO("Build with debugging enabled")
	 AS_ECHO("DEBUG_CFLAGS   : $DEBUG_CFLAGS")])
AS_ECHO()
AS_IF([test "x$QUALITY_CFLAGS" = x],
	[AS_ECHO("Compiler checks disabled")],
	[AS_ECHO("Build with compiler checks enabled")
	 AS_ECHO("QUALITY_CFLAGS : $QUALITY_CFLAGS")])
AS_ECHO()
AS_ECHO("configure complete. Now run 'make'")
AS_ECHO()

dnl The End.
