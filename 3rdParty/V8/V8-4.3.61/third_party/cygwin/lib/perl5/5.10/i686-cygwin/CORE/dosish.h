/*    dosish.h
 *
 *    Copyright (C) 1993, 1994, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */
#define ABORT() abort();

#ifndef SH_PATH
#define SH_PATH "/bin/sh"
#endif

#ifdef DJGPP
#  define BIT_BUCKET "nul"
#  define OP_BINARY O_BINARY
#  define PERL_SYS_INIT_BODY(c,v)					\
	 MALLOC_CHECK_TAINT2(*c,*v) Perl_DJGPP_init(c,v); PERLIO_INIT
#  define init_os_extras Perl_init_os_extras
#  define HAS_UTIME
#  define HAS_KILL
   char *djgpp_pathexp (const char*);
   void Perl_DJGPP_init (int *argcp,char ***argvp);
#  if (DJGPP==2 && DJGPP_MINOR < 2)
#    define NO_LOCALECONV_MON_THOUSANDS_SEP
#  endif
#  define PERL_FS_VER_FMT	"%d_%d_%d"
#else	/* DJGPP */
#  ifdef WIN32
#    define PERL_SYS_INIT_BODY(c,v)					\
	MALLOC_CHECK_TAINT2(*c,*v) Perl_win32_init(c,v); PERLIO_INIT
#    define PERL_SYS_TERM_BODY()   Perl_win32_term()
#    define BIT_BUCKET "nul"
#  else
#	 ifdef NETWARE
#      define PERL_SYS_INIT_BODY(c,v)					\
	MALLOC_CHECK_TAINT2(*c,*v) Perl_nw5_init(c,v); PERLIO_INIT
#      define BIT_BUCKET "nwnul"
#    else
#      define PERL_SYS_INIT_BODY(c,v)		\
	MALLOC_CHECK_TAINT2(*c,*v); PERLIO_INIT
#      define BIT_BUCKET "\\dev\\nul" /* "wanna be like, umm, Newlined, or somethin?" */
#    endif /* NETWARE */
#  endif
#endif	/* DJGPP */

#ifndef PERL_SYS_TERM_BODY
#  define PERL_SYS_TERM_BODY() HINTS_REFCNT_TERM; OP_REFCNT_TERM; PERLIO_TERM; MALLOC_TERM
#endif
#define dXSUB_SYS

/*
 * 5.003_07 and earlier keyed on #ifdef MSDOS for determining if we were 
 * running on DOS, *and* if we had to cope with 16 bit memory addressing 
 * constraints, *and* we need to have memory allocated as unsigned long.
 *
 * with the advent of *real* compilers for DOS, they are not locked together.
 * MSDOS means "I am running on MSDOS". HAS_64K_LIMIT means "I have 
 * 16 bit memory addressing constraints".
 *
 * if you need the last, try #DEFINE MEM_SIZE unsigned long.
 */
#ifdef MSDOS
#  ifndef DJGPP
#    define HAS_64K_LIMIT
#  endif
#endif

/* USEMYBINMODE
 *	This symbol, if defined, indicates that the program should
 *	use the routine my_binmode(FILE *fp, char iotype, int mode) to insure
 *	that a file is in "binary" mode -- that is, that no translation
 *	of bytes occurs on read or write operations.
 */
#undef USEMYBINMODE

/* Stat_t:
 *	This symbol holds the type used to declare buffers for information
 *	returned by stat().  It's usually just struct stat.  It may be necessary
 *	to include <sys/stat.h> and <sys/types.h> to get any typedef'ed
 *	information.
 */
#if defined(WIN64) || defined(USE_LARGE_FILES)
# if defined(__BORLANDC__) /* buk */
#  include <sys\stat.h>
#  define Stat_t struct stati64
# else
#define Stat_t struct _stati64
# endif
#else
#if defined(UNDER_CE)
#define Stat_t struct xcestat
#else
#define Stat_t struct stat
#endif
#endif

/* USE_STAT_RDEV:
 *	This symbol is defined if this system has a stat structure declaring
 *	st_rdev
 */
#define USE_STAT_RDEV 	/**/

/* ACME_MESS:
 *	This symbol, if defined, indicates that error messages should be 
 *	should be generated in a format that allows the use of the Acme
 *	GUI/editor's autofind feature.
 */
#undef ACME_MESS	/**/

/* ALTERNATE_SHEBANG:
 *	This symbol, if defined, contains a "magic" string which may be used
 *	as the first line of a Perl program designed to be executed directly
 *	by name, instead of the standard Unix #!.  If ALTERNATE_SHEBANG
 *	begins with a character other then #, then Perl will only treat
 *	it as a command line if it finds the string "perl" in the first
 *	word; otherwise it's treated as the first line of code in the script.
 *	(IOW, Perl won't hand off to another interpreter via an alternate
 *	shebang sequence that might be legal Perl code.)
 */
/* #define ALTERNATE_SHEBANG "#!" / **/

#include <signal.h>

/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 fwrite

#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#ifdef DJGPP
#   define Fflush(fp)      djgpp_fflush(fp)
#else
#   define Fflush(fp)      fflush(fp)
#endif
#define Mkdir(path,mode)   mkdir((path),(mode))

#ifndef WIN32
#  define Stat(fname,bufptr) stat((fname),(bufptr))
#else
#  define HAS_IOCTL
#  define HAS_UTIME
#  define HAS_KILL
#  define HAS_WAIT
#  define HAS_CHOWN
#endif	/* WIN32 */

/*
 * <rich@phekda.freeserve.co.uk>: The DJGPP port has code that converts
 * the return code of system() into the form that Unixy wait usually
 * returns:
 *
 * - signal number in bits 0-6;
 * - core dump flag in bit 7;
 * - exit code in bits 8-15.
 *
 * Bits 0-7 are always zero for DJGPP, because it uses system().
 * See djgpp.c.
 *
 * POSIX::W* use the W* macros from <sys/wait.h> to decode
 * the return code. Unfortunately the W* macros for DJGPP use
 * a different format than Unixy wait does. So there's a mismatch
 * and, say, WEXITSTATUS($?) will return bogus values.
 *
 * So here we add hack to redefine the W* macros from DJGPP's <sys/wait.h>
 * to work with our return-code conversion.
 */

#ifdef DJGPP

#include <sys/wait.h>

#undef WEXITSTATUS
#undef WIFEXITED
#undef WIFSIGNALED
#undef WIFSTOPPED
#undef WNOHANG
#undef WSTOPSIG
#undef WTERMSIG
#undef WUNTRACED

#define WEXITSTATUS(stat_val) ((stat_val) >> 8)
#define WIFEXITED(stat_val)   0
#define WIFSIGNALED(stat_val) 0
#define WIFSTOPPED(stat_val)  0
#define WNOHANG               0
#define WSTOPSIG(stat_val)    0
#define WTERMSIG(stat_val)    0
#define WUNTRACED             0

#endif

/* Don't go reading from /dev/urandom */
#define PERL_NO_DEV_RANDOM

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
