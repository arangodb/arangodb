/*    perlvars.h
 *
 *    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/****************/
/* Truly global */
/****************/

/* Don't forget to re-run embed.pl to propagate changes! */

/* This file describes the "global" variables used by perl
 * This used to be in perl.h directly but we want to abstract out into
 * distinct files which are per-thread, per-interpreter or really global,
 * and how they're initialized.
 *
 * The 'G' prefix is only needed for vars that need appropriate #defines
 * generated in embed*.h.  Such symbols are also used to generate
 * the appropriate export list for win32. */

/* global state */
PERLVAR(Gcurinterp,	PerlInterpreter *)
					/* currently running interpreter
					 * (initial parent interpreter under
					 * useithreads) */
#if defined(USE_ITHREADS)
PERLVAR(Gthr_key,	perl_key)	/* key to retrieve per-thread struct */
#endif

/* constants (these are not literals to facilitate pointer comparisons)
 * (PERLVARISC really does create variables, despite its looks) */
PERLVARISC(GYes,	"1")
PERLVARISC(GNo,		"")
PERLVARISC(Ghexdigit,	"0123456789abcdef0123456789ABCDEF")
PERLVARISC(Gpatleave,	"\\.^$@dDwWsSbB+*?|()-nrtfeaxc0123456789[{]}")

/* XXX does anyone even use this? */
PERLVARI(Gdo_undump,	bool,	FALSE)	/* -u or dump seen? */

#if defined(MYMALLOC) && defined(USE_ITHREADS)
PERLVAR(Gmalloc_mutex,	perl_mutex)	/* Mutex for malloc */
#endif

#if defined(USE_ITHREADS)
PERLVAR(Gop_mutex,	perl_mutex)	/* Mutex for op refcounting */
#endif

#ifdef USE_ITHREADS
PERLVAR(Gdollarzero_mutex, perl_mutex)	/* Modifying $0 */
#endif


/* This is constant on most architectures, a global on OS/2 */
#ifdef OS2
#  define PERL___C
#else
#  define PERL___C const
#endif
PERLVARI(Gsh_path,	PERL___C char *, SH_PATH) /* full path of shell */
#undef PERL___C

#ifndef PERL_MICRO
/* If Perl has to ignore SIGPFE, this is its saved state.
 * See perl.h macros PERL_FPU_INIT and PERL_FPU_{PRE,POST}_EXEC. */
PERLVAR(Gsigfpe_saved,	Sighandler_t)
#endif

/* Restricted hashes placeholder value.
 * The contents are never used, only the address. */
PERLVAR(Gsv_placeholder, SV)

#ifndef PERL_MICRO
PERLVARI(Gcsighandlerp,	Sighandler_t, Perl_csighandler)	/* Pointer to C-level sighandler */
#endif

#ifndef PERL_USE_SAFE_PUTENV
PERLVARI(Guse_safe_putenv, int, 1)
#endif

#ifdef USE_PERLIO
PERLVARI(Gperlio_fd_refcnt, int*, 0) /* Pointer to array of fd refcounts.  */
PERLVARI(Gperlio_fd_refcnt_size, int, 0) /* Size of the array */
PERLVARI(Gperlio_debug_fd, int, 0) /* the fd to write perlio debug into, 0 means not set yet */
#endif

#ifdef HAS_MMAP
PERLVARI(Gmmap_page_size, IV, 0)
#endif

#if defined(FAKE_PERSISTENT_SIGNAL_HANDLERS)||defined(FAKE_DEFAULT_SIGNAL_HANDLERS)
PERLVARI(Gsig_handlers_initted, int, 0)
#endif
#ifdef FAKE_PERSISTENT_SIGNAL_HANDLERS
PERLVARA(Gsig_ignoring, SIG_SIZE, int)	/* which signals we are ignoring */
#endif
#ifdef FAKE_DEFAULT_SIGNAL_HANDLERS
PERLVARA(Gsig_defaulting, SIG_SIZE, int)
#endif

#ifndef PERL_IMPLICIT_CONTEXT
PERLVAR(Gsig_sv, SV*)
#endif

/* XXX signals are process-wide anyway, so we
 * ignore the implications of this for threading */
#ifndef HAS_SIGACTION
PERLVARI(Gsig_trapped, int, 0)
#endif

#ifdef DEBUGGING
PERLVAR(Gwatch_pvx, char*)
#endif

#ifdef PERL_GLOBAL_STRUCT 
PERLVAR(Gppaddr, Perl_ppaddr_t*) /* or opcode.h */
PERLVAR(Gcheck,  Perl_check_t *) /* or opcode.h */
PERLVARA(Gfold_locale, 256, unsigned char) /* or perl.h */
#endif

#ifdef PERL_NEED_APPCTX
PERLVAR(Gappctx, void*) /* the application context */
#endif

PERLVAR(Gop_sequence, HV*) /* dump.c */
PERLVARI(Gop_seq, UV, 0) /* dump.c */

#if defined(HAS_TIMES) && defined(PERL_NEED_TIMESBASE)
PERLVAR(Gtimesbase, struct tms)
#endif

/* allocate a unique index to every module that calls MY_CXT_INIT */

#ifdef PERL_IMPLICIT_CONTEXT
# ifdef USE_ITHREADS
PERLVAR(Gmy_ctx_mutex, perl_mutex)
# endif
PERLVARI(Gmy_cxt_index, int, 0)
#endif

#if defined(USE_ITHREADS)
PERLVAR(Ghints_mutex, perl_mutex)    /* Mutex for refcounted he refcounting */
#endif

#if defined(USE_ITHREADS)
PERLVAR(Gperlio_mutex, perl_mutex)    /* Mutex for perlio fd refcounts */
#endif

/* this is currently set without MUTEX protection, so keep it a type which
 * can be set atomically (ie not a bit field) */
PERLVARI(Gveto_cleanup,	int, FALSE)	/* exit without cleanup */

/* dummy variables that hold pointers to both runops functions, thus forcing
 * them *both* to get linked in (useful for Peek.xs, debugging etc) */

PERLVARI(Grunops_std,	runops_proc_t,	MEMBER_TO_FPTR(Perl_runops_standard))
PERLVARI(Grunops_dbg,	runops_proc_t,	MEMBER_TO_FPTR(Perl_runops_debug))


/* These are baked at compile time into any shared perl library.
   In future 5.10.x releases this will allow us in main() to sanity test the
   library we're linking against.  */

PERLVARI(Grevision,	U8,	PERL_REVISION)
PERLVARI(Gversion,	U8,	PERL_VERSION)
PERLVARI(Gsubversion,	U8,	PERL_SUBVERSION)

#if defined(MULTIPLICITY)
#  define PERL_INTERPRETER_SIZE_UPTO_MEMBER(member)			\
    STRUCT_OFFSET(struct interpreter, member) +				\
    sizeof(((struct interpreter*)0)->member)

/* These might be useful.  */
PERLVARI(Ginterp_size,	U16,	sizeof(struct interpreter))
#if defined(PERL_GLOBAL_STRUCT)
PERLVARI(Gglobal_struct_size,	U16,	sizeof(struct perl_vars))
#endif

/* This will be useful for subsequent releases, because this has to be the
   same in your libperl as in main(), else you have a mismatch and must abort.
*/
PERLVARI(Ginterp_size_5_10_0, U16,
	 PERL_INTERPRETER_SIZE_UPTO_MEMBER(PERL_LAST_5_10_0_INTERP_MEMBER))
#endif
