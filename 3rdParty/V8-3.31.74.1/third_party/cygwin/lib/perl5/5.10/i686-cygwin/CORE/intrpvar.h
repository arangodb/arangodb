/*   intrpvar.h 
 *
 *    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
 *    2006, 2007
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
=head1 Per-Interpreter Variables
*/

/* These variables are per-interpreter in threaded/multiplicity builds,
 * global otherwise.

 * Don't forget to re-run embed.pl to propagate changes! */

/* New variables must be added to the very end for binary compatibility.
 * XSUB.h provides wrapper functions via perlapi.h that make this
 * irrelevant, but not all code may be expected to #include XSUB.h. */

/* Don't forget to add your variable also to perl_clone()! */

/* The 'I' prefix is only needed for vars that need appropriate #defines
 * generated when built with or without MULTIPLICITY.  It is also used
 * to generate the appropriate export list for win32.
 *
 * When building without MULTIPLICITY, these variables will be truly global.
 *
 * Important ones in the first cache line (if alignment is done right) */

PERLVAR(Istack_sp,	SV **)		/* top of the stack */
#ifdef OP_IN_REGISTER
PERLVAR(Iopsave,	OP *)
#else
PERLVAR(Iop,		OP *)		/* currently executing op */
#endif
PERLVAR(Icurpad,	SV **)		/* active pad (lexicals+tmps) */

PERLVAR(Istack_base,	SV **)
PERLVAR(Istack_max,	SV **)

PERLVAR(Iscopestack,	I32 *)		/* scopes we've ENTERed */
PERLVAR(Iscopestack_ix,	I32)
PERLVAR(Iscopestack_max,I32)

PERLVAR(Isavestack,	ANY *)		/* items that need to be restored when
					   LEAVEing scopes we've ENTERed */
PERLVAR(Isavestack_ix,	I32)
PERLVAR(Isavestack_max,	I32)

PERLVAR(Itmps_stack,	SV **)		/* mortals we've made */
PERLVARI(Itmps_ix,	I32,	-1)
PERLVARI(Itmps_floor,	I32,	-1)
PERLVAR(Itmps_max,	I32)
PERLVAR(Imodcount,	I32)		/* how much mod()ification in
					   assignment? */

PERLVAR(Imarkstack,	I32 *)		/* stack_sp locations we're
					   remembering */
PERLVAR(Imarkstack_ptr,	I32 *)
PERLVAR(Imarkstack_max,	I32 *)

PERLVAR(ISv,		SV *)		/* used to hold temporary values */
PERLVAR(IXpv,		XPV *)		/* used to hold temporary values */

/*
=for apidoc Amn|STRLEN|PL_na

A convenience variable which is typically used with C<SvPV> when one
doesn't care about the length of the string.  It is usually more efficient
to either declare a local variable and use that instead or to use the
C<SvPV_nolen> macro.

=cut
*/

PERLVAR(Ina,		STRLEN)		/* for use in SvPV when length is
					   Not Applicable */

/* stat stuff */
PERLVAR(Istatbuf,	Stat_t)
PERLVAR(Istatcache,	Stat_t)		/* _ */
PERLVAR(Istatgv,	GV *)
PERLVARI(Istatname,	SV *,	NULL)

#ifdef HAS_TIMES
PERLVAR(Itimesbuf,	struct tms)
#endif

/* Fields used by magic variables such as $@, $/ and so on */
PERLVAR(Icurpm,		PMOP *)		/* what to do \ interps in REs from */

/*
=for apidoc mn|SV*|PL_rs

The input record separator - C<$/> in Perl space.

=for apidoc mn|GV*|PL_last_in_gv

The GV which was last used for a filehandle input operation. (C<< <FH> >>)

=for apidoc mn|SV*|PL_ofs_sv

The output field separator - C<$,> in Perl space.

=cut
*/

PERLVAR(Irs,		SV *)		/* input record separator $/ */
PERLVAR(Ilast_in_gv,	GV *)		/* GV used in last <FH> */
PERLVAR(Iofs_sv,	SV *)		/* output field separator $, */
PERLVAR(Idefoutgv,	GV *)		/* default FH for output */
PERLVARI(Ichopset,	const char *, " \n-")	/* $: */
PERLVAR(Iformtarget,	SV *)
PERLVAR(Ibodytarget,	SV *)
PERLVAR(Itoptarget,	SV *)

/* Stashes */
PERLVAR(Idefstash,	HV *)		/* main symbol table */
PERLVAR(Icurstash,	HV *)		/* symbol table for current package */

PERLVAR(Irestartop,	OP *)		/* propagating an error from croak? */
PERLVAR(Icurcop,	COP * VOL)
PERLVAR(Icurstack,	AV *)		/* THE STACK */
PERLVAR(Icurstackinfo,	PERL_SI *)	/* current stack + context */
PERLVAR(Imainstack,	AV *)		/* the stack when nothing funny is
					   happening */

PERLVAR(Itop_env,	JMPENV *)	/* ptr to current sigjmp environment */
PERLVAR(Istart_env,	JMPENV)		/* empty startup sigjmp environment */
PERLVARI(Ierrors,	SV *,	NULL)	/* outstanding queued errors */

/* statics "owned" by various functions */
PERLVAR(Ihv_fetch_ent_mh, HE*)		/* owned by hv_fetch_ent() */

PERLVAR(Ilastgotoprobe,	OP*)		/* from pp_ctl.c */

/* sort stuff */
PERLVAR(Isortcop,	OP *)		/* user defined sort routine */
PERLVAR(Isortstash,	HV *)		/* which is in some package or other */
PERLVAR(Ifirstgv,	GV *)		/* $a */
PERLVAR(Isecondgv,	GV *)		/* $b */

/* float buffer */
PERLVAR(Iefloatbuf,	char *)
PERLVAR(Iefloatsize,	STRLEN)

/* regex stuff */

PERLVAR(Iscreamfirst,	I32 *)
PERLVAR(Iscreamnext,	I32 *)
PERLVAR(Ilastscream,	SV *)

PERLVAR(Ireg_state,	struct re_save_state)

PERLVAR(Iregdummy,	regnode)	/* from regcomp.c */

PERLVARI(Idumpindent,	U16,	4)	/* number of blanks per dump
					   indentation level */


PERLVAR(Iutf8locale,	bool)		/* utf8 locale detected */
PERLVARI(Irehash_seed_set, bool, FALSE)	/* 582 hash initialized? */

PERLVARA(Icolors,6,	char *)		/* from regcomp.c */

PERLVARI(Ipeepp,	peep_t, MEMBER_TO_FPTR(Perl_peep))
					/* Pointer to peephole optimizer */

PERLVARI(Imaxscream,	I32,	-1)
PERLVARI(Ireginterp_cnt,I32,	 0)	/* Whether "Regexp" was interpolated. */
PERLVARI(Iwatchaddr,	char **, 0)
PERLVAR(Iwatchok,	char *)

/* the currently active slab in a chain of slabs of regmatch states,
 * and the currently active state within that slab */

PERLVARI(Iregmatch_slab, regmatch_slab *,	NULL)
PERLVAR(Iregmatch_state, regmatch_state *)

/* Put anything new that is pointer aligned here. */

PERLVAR(Idelaymagic,	U16)		/* ($<,$>) = ... */
PERLVAR(Ilocalizing,	U8)		/* are we processing a local() list? */
PERLVAR(Icolorset,	bool)		/* from regcomp.c */
PERLVARI(Idirty,	bool, FALSE)	/* in the middle of tearing things
					   down? */
PERLVAR(Iin_eval,	VOL U8)		/* trap "fatal" errors? */
PERLVAR(Itainted,	bool)		/* using variables controlled by $< */

/* This value may be set when embedding for full cleanup  */
/* 0=none, 1=full, 2=full with checks */
/* mod_perl is special, and also assigns a meaning -1 */
PERLVARI(Iperl_destruct_level,	signed char,	0)

PERLVAR(Iperldb,	U32)

/* pseudo environmental stuff */
PERLVAR(Iorigargc,	int)
PERLVAR(Iorigargv,	char **)
PERLVAR(Ienvgv,		GV *)
PERLVAR(Iincgv,		GV *)
PERLVAR(Ihintgv,	GV *)
PERLVAR(Iorigfilename,	char *)
PERLVAR(Idiehook,	SV *)
PERLVAR(Iwarnhook,	SV *)

/* switches */
PERLVAR(Ipatchlevel,	SV *)
PERLVAR(Ilocalpatches,	const char * const *)
PERLVARI(Isplitstr,	const char *, " ")

PERLVAR(Iminus_c,	bool)
PERLVAR(Ipreprocess,	bool)
PERLVAR(Iminus_n,	bool)
PERLVAR(Iminus_p,	bool)
PERLVAR(Iminus_l,	bool)
PERLVAR(Iminus_a,	bool)
PERLVAR(Iminus_F,	bool)
PERLVAR(Idoswitches,	bool)

PERLVAR(Iminus_E,	bool)

/*

=for apidoc mn|bool|PL_dowarn

The C variable which corresponds to Perl's $^W warning variable.

=cut
*/

PERLVAR(Idowarn,	U8)
PERLVAR(Idoextract,	bool)
PERLVAR(Isawampersand,	bool)		/* must save all match strings */
PERLVAR(Iunsafe,	bool)
PERLVAR(Iexit_flags,	U8)		/* was exit() unexpected, etc. */
PERLVAR(Isrand_called,	bool)
/* Part of internal state, but makes the 16th 1 byte variable in a row.  */
PERLVAR(Itainting,	bool)		/* doing taint checks */
PERLVAR(Iinplace,	char *)
PERLVAR(Ie_script,	SV *)

/* magical thingies */
PERLVAR(Ibasetime,	Time_t)		/* $^T */
PERLVAR(Iformfeed,	SV *)		/* $^L */


PERLVARI(Imaxsysfd,	I32,	MAXSYSFD)
					/* top fd to pass to subprocesses */
PERLVAR(Istatusvalue,	I32)		/* $? */
#ifdef VMS
PERLVAR(Istatusvalue_vms,U32)
#else
PERLVAR(Istatusvalue_posix,I32)
#endif

PERLVARI(Isig_pending, int,0)           /* Number if highest signal pending */
PERLVAR(Ipsig_pend, int *)		/* per-signal "count" of pending */

/* shortcuts to various I/O objects */
PERLVAR(Istdingv,	GV *)
PERLVAR(Istderrgv,	GV *)
PERLVAR(Idefgv,		GV *)
PERLVAR(Iargvgv,	GV *)
PERLVAR(Iargvoutgv,	GV *)
PERLVAR(Iargvout_stack,	AV *)

/* shortcuts to regexp stuff */
PERLVAR(Ireplgv,	GV *)

/* shortcuts to misc objects */
PERLVAR(Ierrgv,		GV *)

/* shortcuts to debugging objects */
PERLVAR(IDBgv,		GV *)
PERLVAR(IDBline,	GV *)

/*
=for apidoc mn|GV *|PL_DBsub
When Perl is run in debugging mode, with the B<-d> switch, this GV contains
the SV which holds the name of the sub being debugged.  This is the C
variable which corresponds to Perl's $DB::sub variable.  See
C<PL_DBsingle>.

=for apidoc mn|SV *|PL_DBsingle
When Perl is run in debugging mode, with the B<-d> switch, this SV is a
boolean which indicates whether subs are being single-stepped.
Single-stepping is automatically turned on after every step.  This is the C
variable which corresponds to Perl's $DB::single variable.  See
C<PL_DBsub>.

=for apidoc mn|SV *|PL_DBtrace
Trace variable used when Perl is run in debugging mode, with the B<-d>
switch.  This is the C variable which corresponds to Perl's $DB::trace
variable.  See C<PL_DBsingle>.

=cut
*/

PERLVAR(IDBsub,		GV *)
PERLVAR(IDBsingle,	SV *)
PERLVAR(IDBtrace,	SV *)
PERLVAR(IDBsignal,	SV *)
PERLVAR(Idbargs,	AV *)		/* args to call listed by caller function */

/* symbol tables */
PERLVAR(Idebstash,	HV *)		/* symbol table for perldb package */
PERLVAR(Iglobalstash,	HV *)		/* global keyword overrides imported here */
PERLVAR(Icurstname,	SV *)		/* name of current package */
PERLVAR(Ibeginav,	AV *)		/* names of BEGIN subroutines */
PERLVAR(Iendav,		AV *)		/* names of END subroutines */
PERLVAR(Iunitcheckav,	AV *)		/* names of UNITCHECK subroutines */
PERLVAR(Icheckav,	AV *)		/* names of CHECK subroutines */
PERLVAR(Iinitav,	AV *)		/* names of INIT subroutines */
PERLVAR(Istrtab,	HV *)		/* shared string table */
PERLVARI(Isub_generation,U32,1)		/* incr to invalidate method cache */

/* funky return mechanisms */
PERLVAR(Iforkprocess,	int)		/* so do_open |- can return proc# */

/* memory management */
PERLVAR(Isv_count,	I32)		/* how many SV* are currently allocated */
PERLVAR(Isv_objcount,	I32)		/* how many objects are currently allocated */
PERLVAR(Isv_root,	SV*)		/* storage for SVs belonging to interp */
PERLVAR(Isv_arenaroot,	SV*)		/* list of areas for garbage collection */

/* subprocess state */
PERLVAR(Ifdpid,		AV *)		/* keep fd-to-pid mappings for my_popen */

/* internal state */
PERLVARI(Iop_mask,	char *,	NULL)	/* masked operations for safe evals */

/* current interpreter roots */
PERLVAR(Imain_cv,	CV *)
PERLVAR(Imain_root,	OP *)
PERLVAR(Imain_start,	OP *)
PERLVAR(Ieval_root,	OP *)
PERLVAR(Ieval_start,	OP *)

/* runtime control stuff */
PERLVARI(Icurcopdb,	COP *,	NULL)

PERLVAR(Ifilemode,	int)		/* so nextargv() can preserve mode */
PERLVAR(Ilastfd,	int)		/* what to preserve mode on */
PERLVAR(Ioldname,	char *)		/* what to preserve mode on */
PERLVAR(IArgv,		char **)	/* stuff to free from do_aexec, vfork safe */
PERLVAR(ICmd,		char *)		/* stuff to free from do_aexec, vfork safe */
/* Elements in this array have ';' appended and are injected as a single line
   into the tokeniser. You can't put any (literal) newlines into any program
   you stuff in into this array, as the point where it's injected is expecting
   a single physical line. */
PERLVAR(Ipreambleav,	AV *)
PERLVAR(Imess_sv,	SV *)
PERLVAR(Iors_sv,	SV *)		/* output record separator $\ */
/* statics moved here for shared library purposes */
PERLVARI(Igensym,	I32,	0)	/* next symbol for getsym() to define */
PERLVARI(Icv_has_eval, bool, FALSE) /* PL_compcv includes an entereval or similar */
PERLVAR(Itaint_warn,	bool)      /* taint warns instead of dying */
PERLVARI(Ilaststype,	U16,	OP_STAT)
PERLVARI(Ilaststatval,	int,	-1)

/* interpreter atexit processing */
PERLVARI(Iexitlistlen,	I32, 0)		/* length of same */
PERLVARI(Iexitlist,	PerlExitListEntry *, NULL)
					/* list of exit functions */

/*
=for apidoc Amn|HV*|PL_modglobal

C<PL_modglobal> is a general purpose, interpreter global HV for use by
extensions that need to keep information on a per-interpreter basis.
In a pinch, it can also be used as a symbol table for extensions
to share data among each other.  It is a good idea to use keys
prefixed by the package name of the extension that owns the data.

=cut
*/

PERLVAR(Imodglobal,	HV *)		/* per-interp module data */

/* these used to be in global before 5.004_68 */
PERLVARI(Iprofiledata,	U32 *,	NULL)	/* table of ops, counts */

PERLVAR(Icompiling,	COP)		/* compiling/done executing marker */

PERLVAR(Icompcv,	CV *)		/* currently compiling subroutine */
PERLVAR(Icomppad,	AV *)		/* storage for lexically scoped temporaries */
PERLVAR(Icomppad_name,	AV *)		/* variable names for "my" variables */
PERLVAR(Icomppad_name_fill,	I32)	/* last "introduced" variable offset */
PERLVAR(Icomppad_name_floor,	I32)	/* start of vars in innermost block */

#ifdef HAVE_INTERP_INTERN
PERLVAR(Isys_intern,	struct interp_intern)
					/* platform internals */
#endif

/* more statics moved here */
PERLVAR(IDBcv,		CV *)		/* from perl.c */
PERLVARI(Igeneration,	int,	100)	/* from op.c */

PERLVARI(Iin_clean_objs,bool,    FALSE)	/* from sv.c */
PERLVARI(Iin_clean_all,	bool,    FALSE)	/* from sv.c */
PERLVAR(Inomemok,	bool)		/* let malloc context handle nomem */
PERLVARI(Isavebegin,     bool,	FALSE)	/* save BEGINs for compiler	*/

PERLVAR(Iuid,		Uid_t)		/* current real user id */
PERLVAR(Ieuid,		Uid_t)		/* current effective user id */
PERLVAR(Igid,		Gid_t)		/* current real group id */
PERLVAR(Iegid,		Gid_t)		/* current effective group id */
PERLVARI(Ian,		U32,	0)	/* malloc sequence number */
PERLVARI(Icop_seqmax,	U32,	0)	/* statement sequence number */
PERLVARI(Ievalseq,	U32,	0)	/* eval sequence number */
PERLVAR(Iorigalen,	U32)
PERLVAR(Iorigenviron,	char **)
#ifdef PERL_USES_PL_PIDSTATUS
PERLVAR(Ipidstatus,	HV *)		/* pid-to-status mappings for waitpid */
#endif
PERLVAR(Iosname,	char *)		/* operating system */

PERLVAR(Isighandlerp,	Sighandler_t)

PERLVARA(Ibody_roots,	PERL_ARENA_ROOTS_SIZE, void*) /* array of body roots */

PERLVAR(Inice_chunk,	char *)		/* a nice chunk of memory to reuse */
PERLVAR(Inice_chunk_size,	U32)	/* how nice the chunk of memory is */

PERLVARI(Imaxo,	int,	MAXO)		/* maximum number of ops */

PERLVARI(Irunops,	runops_proc_t,	MEMBER_TO_FPTR(RUNOPS_DEFAULT))

/*
=for apidoc Amn|SV|PL_sv_undef
This is the C<undef> SV.  Always refer to this as C<&PL_sv_undef>.

=for apidoc Amn|SV|PL_sv_no
This is the C<false> SV.  See C<PL_sv_yes>.  Always refer to this as
C<&PL_sv_no>.

=for apidoc Amn|SV|PL_sv_yes
This is the C<true> SV.  See C<PL_sv_no>.  Always refer to this as
C<&PL_sv_yes>.

=cut
*/

PERLVAR(Isv_undef,	SV)
PERLVAR(Isv_no,		SV)
PERLVAR(Isv_yes,	SV)

PERLVAR(Isubname,	SV *)		/* name of current subroutine */

PERLVAR(Isubline,	I32)		/* line this subroutine began on */
PERLVAR(Imin_intro_pending,	I32)	/* start of vars to introduce */

PERLVAR(Imax_intro_pending,	I32)	/* end of vars to introduce */
PERLVAR(Ipadix,		I32)		/* max used index in current "register" pad */

PERLVAR(Ipadix_floor,	I32)		/* how low may inner block reset padix */
PERLVAR(Ipad_reset_pending,	I32)	/* reset pad on next attempted alloc */

PERLVAR(Ihints,		U32)		/* pragma-tic compile-time flags */

PERLVAR(Idebug,		VOL U32)	/* flags given to -D switch */

PERLVARI(Iamagic_generation,	long,	0)

#ifdef USE_LOCALE_COLLATE
PERLVAR(Icollation_name,char *)		/* Name of current collation */
PERLVAR(Icollxfrm_base,	Size_t)		/* Basic overhead in *xfrm() */
PERLVARI(Icollxfrm_mult,Size_t,	2)	/* Expansion factor in *xfrm() */
PERLVARI(Icollation_ix,	U32,	0)	/* Collation generation index */
PERLVARI(Icollation_standard, bool,	TRUE)
					/* Assume simple collation */
#endif /* USE_LOCALE_COLLATE */


#if defined (PERL_UTF8_CACHE_ASSERT) || defined (DEBUGGING)
#  define PERL___I -1
#else
#  define PERL___I 1
#endif
PERLVARI(Iutf8cache, I8, PERL___I)	/* Is the utf8 caching code enabled? */
#undef PERL___I


#ifdef USE_LOCALE_NUMERIC

PERLVARI(Inumeric_standard,	bool,	TRUE)
					/* Assume simple numerics */
PERLVARI(Inumeric_local,	bool,	TRUE)
					/* Assume local numerics */
PERLVAR(Inumeric_name,	char *)		/* Name of current numeric locale */
#endif /* !USE_LOCALE_NUMERIC */

/* utf8 character classes */
PERLVAR(Iutf8_alnum,	SV *)
PERLVAR(Iutf8_alnumc,	SV *)
PERLVAR(Iutf8_ascii,	SV *)
PERLVAR(Iutf8_alpha,	SV *)
PERLVAR(Iutf8_space,	SV *)
PERLVAR(Iutf8_cntrl,	SV *)
PERLVAR(Iutf8_graph,	SV *)
PERLVAR(Iutf8_digit,	SV *)
PERLVAR(Iutf8_upper,	SV *)
PERLVAR(Iutf8_lower,	SV *)
PERLVAR(Iutf8_print,	SV *)
PERLVAR(Iutf8_punct,	SV *)
PERLVAR(Iutf8_xdigit,	SV *)
PERLVAR(Iutf8_mark,	SV *)
PERLVAR(Iutf8_toupper,	SV *)
PERLVAR(Iutf8_totitle,	SV *)
PERLVAR(Iutf8_tolower,	SV *)
PERLVAR(Iutf8_tofold,	SV *)
PERLVAR(Ilast_swash_hv,	HV *)
PERLVAR(Ilast_swash_tmps,	U8 *)
PERLVAR(Ilast_swash_slen,	STRLEN)
PERLVARA(Ilast_swash_key,10,	U8)
PERLVAR(Ilast_swash_klen,	U8)	/* Only needs to store 0-10  */

#ifdef FCRYPT
PERLVARI(Icryptseen,	bool,	FALSE)	/* has fast crypt() been initialized? */
#endif

PERLVARI(Iglob_index,	int,	0)


PERLVAR(Iparser,	yy_parser *)	/* current parser state */

PERLVAR(Ibitcount,	char *)

PERLVAR(Ipsig_ptr, SV**)
PERLVAR(Ipsig_name, SV**)

#if defined(PERL_IMPLICIT_SYS)
PERLVAR(IMem,		struct IPerlMem*)
PERLVAR(IMemShared,	struct IPerlMem*)
PERLVAR(IMemParse,	struct IPerlMem*)
PERLVAR(IEnv,		struct IPerlEnv*)
PERLVAR(IStdIO,		struct IPerlStdIO*)
PERLVAR(ILIO,		struct IPerlLIO*)
PERLVAR(IDir,		struct IPerlDir*)
PERLVAR(ISock,		struct IPerlSock*)
PERLVAR(IProc,		struct IPerlProc*)
#endif

PERLVAR(Iptr_table,	PTR_TBL_t*)
PERLVARI(Ibeginav_save, AV*, NULL)	/* save BEGIN{}s when compiling */

PERLVAR(Ibody_arenas, void*) /* pointer to list of body-arenas */


#ifdef USE_LOCALE_NUMERIC

PERLVAR(Inumeric_radix_sv,	SV *)	/* The radix separator if not '.' */

#endif

#if defined(USE_ITHREADS)
PERLVAR(Iregex_pad,     SV**)		/* Shortcut into the array of
					   regex_padav */
PERLVAR(Iregex_padav,   AV*)		/* All regex objects, indexed via the
					   values in op_pmoffset of pmop.
					   Entry 0 is an array of IVs listing
					   the now-free slots in the array */
#endif

#ifdef USE_REENTRANT_API
PERLVAR(Ireentrant_buffer, REENTR*)	/* here we store the _r buffers */
#endif

PERLVAR(Icustom_op_names, HV*)  /* Names of user defined ops */
PERLVAR(Icustom_op_descs, HV*)  /* Descriptions of user defined ops */

#ifdef PERLIO_LAYERS
PERLVARI(Iperlio, PerlIO *,NULL)
PERLVARI(Iknown_layers, PerlIO_list_t *,NULL)
PERLVARI(Idef_layerlist, PerlIO_list_t *,NULL)
#endif

PERLVARI(Iencoding,	SV*, NULL)		/* character encoding */

PERLVAR(Idebug_pad,	struct perl_debug_pad)	/* always needed because of the re extension */

PERLVAR(Iutf8_idstart,	SV *)
PERLVAR(Iutf8_idcont,	SV *)

PERLVAR(Isort_RealCmp,  SVCOMPARE_t)

PERLVARI(Icheckav_save, AV*, NULL)	/* save CHECK{}s when compiling */
PERLVARI(Iunitcheckav_save, AV*, NULL)	/* save UNITCHECK{}s when compiling */

PERLVARI(Iclocktick, long, 0)	/* this many times() ticks in a second */

PERLVARI(Iin_load_module, int, 0)	/* to prevent recursions in PerlIO_find_layer */

PERLVAR(Iunicode, U32)	/* Unicode features: $ENV{PERL_UNICODE} or -C */

PERLVAR(Isignals, U32)	/* Using which pre-5.8 signals */

PERLVAR(Ireentrant_retint, int)	/* Integer return value from reentrant functions */

PERLVAR(Istashcache,	HV *)		/* Cache to speed up S_method_common */

/* Hooks to shared SVs and locks. */
PERLVARI(Isharehook,	share_proc_t,	MEMBER_TO_FPTR(Perl_sv_nosharing))
PERLVARI(Ilockhook,	share_proc_t,	MEMBER_TO_FPTR(Perl_sv_nosharing))
#ifdef NO_MATHOMS
#  define PERL_UNLOCK_HOOK Perl_sv_nosharing
#else
/* This reference ensures that the mathoms are linked with perl */
#  define PERL_UNLOCK_HOOK Perl_sv_nounlocking
#endif
PERLVARI(Iunlockhook,	share_proc_t,	MEMBER_TO_FPTR(PERL_UNLOCK_HOOK))

PERLVARI(Ithreadhook,	thrhook_proc_t,	MEMBER_TO_FPTR(Perl_nothreadhook))

PERLVARI(Ihash_seed, UV, 0)		/* Hash initializer */

PERLVARI(Irehash_seed, UV, 0)		/* 582 hash initializer */

PERLVARI(Iisarev, HV*, NULL) /* Reverse map of @ISA dependencies */

/* The last unconditional member of the interpreter structure when 5.10.0 was
   released. The offset of the end of this is baked into a global variable in 
   any shared perl library which will allow a sanity test in future perl
   releases.  */
#define PERL_LAST_5_10_0_INTERP_MEMBER	Iisarev

#ifdef PERL_IMPLICIT_CONTEXT
PERLVARI(Imy_cxt_size, int, 0)		/* size of PL_my_cxt_list */
PERLVARI(Imy_cxt_list, void **, NULL) /* per-module array of MY_CXT pointers */
#  ifdef PERL_GLOBAL_STRUCT_PRIVATE
PERLVARI(Imy_cxt_keys, const char **, NULL) /* per-module array of pointers to MY_CXT_KEY constants */
#  endif
#endif

#ifdef PERL_TRACK_MEMPOOL
/* For use with the memory debugging code in util.c  */
PERLVAR(Imemory_debug_header, struct perl_memory_debug_header)
#endif

#ifdef DEBUG_LEAKING_SCALARS_FORK_DUMP
/* File descriptor to talk to the child which dumps scalars.  */
PERLVARI(Idumper_fd, int, -1)
#endif

/* Stores the PPID */
#ifdef THREADS_HAVE_PIDS
PERLVARI(Ippid,		IV,		0)
#endif

#ifdef PERL_MAD
PERLVARI(Imadskills,	bool, FALSE)	/* preserve all syntactic info */
					/* (MAD = Misc Attribute Decoration) */
PERLVARI(Ixmlfp, PerlIO *,NULL)
#endif

#ifdef PL_OP_SLAB_ALLOC
PERLVAR(IOpPtr,I32 **)
PERLVARI(IOpSpace,I32,0)
PERLVAR(IOpSlab,I32 *)
#endif

#ifdef PERL_DEBUG_READONLY_OPS
PERLVARI(Islabs, I32**, NULL)	/* Array of slabs that have been allocated */
PERLVARI(Islab_count, U32, 0)	/* Size of the array */
#endif

/* Can shared object be destroyed */
PERLVARI(Idestroyhook, destroyable_proc_t, MEMBER_TO_FPTR(Perl_sv_destroyable))

/* If you are adding a U8 or U16, check to see if there are 'Space' comments
 * above on where there are gaps which currently will be structure padding.  */

/* Within a stable branch, new variables must be added to the very end, before
 * this comment, for binary compatibility (the offsets of the old members must
 *  not change).
 * (Don't forget to add your variable also to perl_clone()!)
 * XSUB.h provides wrapper functions via perlapi.h that make this
 * irrelevant, but not all code may be expected to #include XSUB.h.
 */
