/* config/config.h.  Generated from config.h.in by configure.  */
/* config/config.h.in.  Generated from configure.ac by autoheader.  */

/* static programs have broken cxa_guard */
/* #undef BROKEN_CXA_GUARD */

/* Define to 1 if you have the `backtrace' function. */
/* #undef HAVE_BACKTRACE */

/* do we have clock_gettime? */
/* #undef HAVE_CLOCK_GETTIME */

/* do we have clock_get_time? */
#define HAVE_CLOCK_GET_TIME 1

/* define if the compiler supports basic C++11 syntax */
/* #undef HAVE_CXX11 */

/* Define to 1 if you have the `futimes' function. */
#define HAVE_FUTIMES 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `tcmalloc' library (-ltcmalloc). */
/* #undef HAVE_LIBTCMALLOC */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <openssl/ssl.h> header file. */
#define HAVE_OPENSSL_SSL_H 1

/* Define if you have POSIX threads libraries and header files. */
/* #undef HAVE_PTHREAD */

/* Define to 1 if you have the <readline/readline.h> header file. */
/* #undef HAVE_READLINE_READLINE_H */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* true if SSLv23_method return SSL_METHOD const* */
/* #undef OPENSSL_NEEDS_CONST */

/* Name of package */
#define PACKAGE "arangodb"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "info@arangodb.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "ArangoDB GmbH"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "ArangoDB GmbH 2.8.0-devel"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "arangodb"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://www.arangodb.com"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.8.0-devel"

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* "" */
#define TRI_CONFIGURE_COMMAND " './configure'"

/* "" */
#define TRI_CONFIGURE_FLAGS ""

/* "" */
#define TRI_CONFIGURE_OPTIONS " '--host=x86_64-apple-darwin' '--target=x86_64-apple-darwin' '--enable-relative' '--enable-maintainer-mode' '--enable-all-in-one-v8' '--enable-all-in-one-libev' '--enable-all-in-one-icu' '--prefix=/arangodb/v1.4.0' '--enable-arangob' 'host_alias=x86_64-apple-darwin' 'target_alias=x86_64-apple-darwin'"

/* true if failure testing ins enabled */
/* #undef TRI_ENABLE_FAILURE_TESTS */

/* true if figures are enabled */
/* #undef TRI_ENABLE_FIGURES */

/* true if high resolution clock should be used */
/* #undef TRI_ENABLE_HIRES_FIGURES */

/* true if logging is enabled */
#define TRI_ENABLE_LOGGER 1

/* true if timing is enabled */
/* #undef TRI_ENABLE_LOGGER_TIMING */

/* true if maintainer mode is enabled */
#define TRI_ENABLE_MAINTAINER_MODE 1

/* true if timing is enabled */
/* #undef TRI_ENABLE_TIMING */

/* true if linenoise is used */
/* #undef TRI_HAVE_LINENOISE */

/* true if readline is used */
/* #undef TRI_HAVE_READLINE */

/* true if we have the linux splice api */
/* #undef TRI_LINUX_SPLICE */

/* "" */
#define TRI_REPOSITORY_VERSION "heads/velocystream-0-g5b2f30407f82605794d7ac7c264aa822f7ebc268"

/* Version number of package */
#define VERSION "2.8.0-devel"

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
/* #undef YYTEXT_POINTER */

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */
