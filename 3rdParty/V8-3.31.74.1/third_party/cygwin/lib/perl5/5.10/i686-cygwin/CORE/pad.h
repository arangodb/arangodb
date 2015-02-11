/*    pad.h
 *
 *    Copyright (C) 2002, 2003, 2005, 2006, 2007 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * This file defines the types and macros associated with the API for
 * manipulating scratchpads, which are used by perl to store lexical
 * variables, op targets and constants.
 */




/* a padlist is currently just an AV; but that might change,
 * so hide the type. Ditto a pad.  */

typedef AV PADLIST;
typedef AV PAD;


/* offsets within a pad */

#if PTRSIZE == 4
typedef U32TYPE PADOFFSET;
#else
#   if PTRSIZE == 8
typedef U64TYPE PADOFFSET;
#   endif
#endif
#define NOT_IN_PAD ((PADOFFSET) -1)

/* B.xs needs these for the benefit of B::Deparse */ 
/* Low range end is exclusive (valid from the cop seq after this one) */
/* High range end is inclusive (valid up to this cop seq) */

#if defined (DEBUGGING) && defined(__GNUC__) && !defined(PERL_GCC_BRACE_GROUPS_FORBIDDEN)
#  define COP_SEQ_RANGE_LOW(sv)						\
	(({ SV *const _svi = (SV *) (sv);				\
	  assert(SvTYPE(_svi) == SVt_NV || SvTYPE(_svi) >= SVt_PVNV);	\
	  assert(SvTYPE(_svi) != SVt_PVAV);				\
	  assert(SvTYPE(_svi) != SVt_PVHV);				\
	  assert(SvTYPE(_svi) != SVt_PVCV);				\
	  assert(SvTYPE(_svi) != SVt_PVFM);				\
	  assert(!isGV_with_GP(_svi));					\
	  ((XPVNV*) SvANY(_svi))->xnv_u.xpad_cop_seq.xlow;		\
	 }))
#  define COP_SEQ_RANGE_HIGH(sv)					\
	(({ SV *const _svi = (SV *) (sv);				\
	  assert(SvTYPE(_svi) == SVt_NV || SvTYPE(_svi) >= SVt_PVNV);	\
	  assert(SvTYPE(_svi) != SVt_PVAV);				\
	  assert(SvTYPE(_svi) != SVt_PVHV);				\
	  assert(SvTYPE(_svi) != SVt_PVCV);				\
	  assert(SvTYPE(_svi) != SVt_PVFM);				\
	  assert(!isGV_with_GP(_svi));					\
	  ((XPVNV*) SvANY(_svi))->xnv_u.xpad_cop_seq.xhigh;		\
	 }))
#  define PARENT_PAD_INDEX(sv)						\
	(({ SV *const _svi = (SV *) (sv);				\
	  assert(SvTYPE(_svi) == SVt_NV || SvTYPE(_svi) >= SVt_PVNV);	\
	  assert(SvTYPE(_svi) != SVt_PVAV);				\
	  assert(SvTYPE(_svi) != SVt_PVHV);				\
	  assert(SvTYPE(_svi) != SVt_PVCV);				\
	  assert(SvTYPE(_svi) != SVt_PVFM);				\
	  assert(!isGV_with_GP(_svi));					\
	  ((XPVNV*) SvANY(_svi))->xnv_u.xpad_cop_seq.xlow;		\
	 }))
#  define PARENT_FAKELEX_FLAGS(sv)					\
	(({ SV *const _svi = (SV *) (sv);				\
	  assert(SvTYPE(_svi) == SVt_NV || SvTYPE(_svi) >= SVt_PVNV);	\
	  assert(SvTYPE(_svi) != SVt_PVAV);				\
	  assert(SvTYPE(_svi) != SVt_PVHV);				\
	  assert(SvTYPE(_svi) != SVt_PVCV);				\
	  assert(SvTYPE(_svi) != SVt_PVFM);				\
	  assert(!isGV_with_GP(_svi));					\
	  ((XPVNV*) SvANY(_svi))->xnv_u.xpad_cop_seq.xhigh;		\
	 }))
#else
#  define COP_SEQ_RANGE_LOW(sv)		\
	(0 + (((XPVNV*) SvANY(sv))->xnv_u.xpad_cop_seq.xlow))
#  define COP_SEQ_RANGE_HIGH(sv)	\
	(0 + (((XPVNV*) SvANY(sv))->xnv_u.xpad_cop_seq.xhigh))


#  define PARENT_PAD_INDEX(sv)		\
	(0 + (((XPVNV*) SvANY(sv))->xnv_u.xpad_cop_seq.xlow))
#  define PARENT_FAKELEX_FLAGS(sv)	\
	(0 + (((XPVNV*) SvANY(sv))->xnv_u.xpad_cop_seq.xhigh))
#endif

/* Flags set in the SvIVX field of FAKE namesvs */
    
#define PAD_FAKELEX_ANON   1 /* the lex is declared in an ANON, or ... */
#define PAD_FAKELEX_MULTI  2 /* the lex can be instantiated multiple times */

/* flags for the pad_new() function */

#define padnew_CLONE	1	/* this pad is for a cloned CV */
#define padnew_SAVE	2	/* save old globals */
#define padnew_SAVESUB	4	/* also save extra stuff for start of sub */

/* values for the pad_tidy() function */

typedef enum {
	padtidy_SUB,		/* tidy up a pad for a sub, */
	padtidy_SUBCLONE,	/* a cloned sub, */
	padtidy_FORMAT		/* or a format */
} padtidy_type;

/* ASSERT_CURPAD_LEGAL and ASSERT_CURPAD_ACTIVE respectively determine
 * whether PL_comppad and PL_curpad are consistent and whether they have
 * active values */

#ifndef PERL_MAD
#  define pad_peg(label)
#endif

#ifdef DEBUGGING
#  define ASSERT_CURPAD_LEGAL(label) \
    pad_peg(label); \
    if (PL_comppad ? (AvARRAY(PL_comppad) != PL_curpad) : (PL_curpad != 0))  \
	Perl_croak(aTHX_ "panic: illegal pad in %s: 0x%"UVxf"[0x%"UVxf"]",\
	    label, PTR2UV(PL_comppad), PTR2UV(PL_curpad));


#  define ASSERT_CURPAD_ACTIVE(label) \
    pad_peg(label); \
    if (!PL_comppad || (AvARRAY(PL_comppad) != PL_curpad))		  \
	Perl_croak(aTHX_ "panic: invalid pad in %s: 0x%"UVxf"[0x%"UVxf"]",\
	    label, PTR2UV(PL_comppad), PTR2UV(PL_curpad));
#else
#  define ASSERT_CURPAD_LEGAL(label)
#  define ASSERT_CURPAD_ACTIVE(label)
#endif



/* Note: the following three macros are actually defined in scope.h, but
 * they are documented here for completeness, since they directly or
 * indirectly affect pads.

=for apidoc m|void|SAVEPADSV	|PADOFFSET po
Save a pad slot (used to restore after an iteration)

XXX DAPM it would make more sense to make the arg a PADOFFSET
=for apidoc m|void|SAVECLEARSV	|SV **svp
Clear the pointed to pad value on scope exit. (i.e. the runtime action of 'my')

=for apidoc m|void|SAVECOMPPAD
save PL_comppad and PL_curpad





=for apidoc m|SV *|PAD_SETSV	|PADOFFSET po|SV* sv
Set the slot at offset C<po> in the current pad to C<sv>

=for apidoc m|void|PAD_SV	|PADOFFSET po
Get the value at offset C<po> in the current pad

=for apidoc m|SV *|PAD_SVl	|PADOFFSET po
Lightweight and lvalue version of C<PAD_SV>.
Get or set the value at offset C<po> in the current pad.
Unlike C<PAD_SV>, does not print diagnostics with -DX.
For internal use only.

=for apidoc m|SV *|PAD_BASE_SV	|PADLIST padlist|PADOFFSET po
Get the value from slot C<po> in the base (DEPTH=1) pad of a padlist

=for apidoc m|void|PAD_SET_CUR	|PADLIST padlist|I32 n
Set the current pad to be pad C<n> in the padlist, saving
the previous current pad. NB currently this macro expands to a string too
long for some compilers, so it's best to replace it with

    SAVECOMPPAD();
    PAD_SET_CUR_NOSAVE(padlist,n);


=for apidoc m|void|PAD_SET_CUR_NOSAVE	|PADLIST padlist|I32 n
like PAD_SET_CUR, but without the save

=for apidoc m|void|PAD_SAVE_SETNULLPAD
Save the current pad then set it to null.

=for apidoc m|void|PAD_SAVE_LOCAL|PAD *opad|PAD *npad
Save the current pad to the local variable opad, then make the
current pad equal to npad

=for apidoc m|void|PAD_RESTORE_LOCAL|PAD *opad
Restore the old pad saved into the local variable opad by PAD_SAVE_LOCAL()

=cut
*/

#ifdef DEBUGGING
#  define PAD_SV(po)	   pad_sv(po)
#  define PAD_SETSV(po,sv) pad_setsv(po,sv)
#else
#  define PAD_SV(po)       (PL_curpad[po])
#  define PAD_SETSV(po,sv) PL_curpad[po] = (sv)
#endif

#define PAD_SVl(po)       (PL_curpad[po])

#define PAD_BASE_SV(padlist, po) \
	(AvARRAY(padlist)[1]) 	\
	    ? AvARRAY((AV*)(AvARRAY(padlist)[1]))[po] : NULL;
    

#define PAD_SET_CUR_NOSAVE(padlist,nth) \
	PL_comppad = (PAD*) (AvARRAY(padlist)[nth]);		\
	PL_curpad = AvARRAY(PL_comppad);			\
	DEBUG_Xv(PerlIO_printf(Perl_debug_log,			\
	      "Pad 0x%"UVxf"[0x%"UVxf"] set_cur    depth=%d\n",	\
	      PTR2UV(PL_comppad), PTR2UV(PL_curpad), (int)(nth)));


#define PAD_SET_CUR(padlist,nth) \
	SAVECOMPPAD();						\
	PAD_SET_CUR_NOSAVE(padlist,nth);


#define PAD_SAVE_SETNULLPAD()	SAVECOMPPAD(); \
	PL_comppad = NULL; PL_curpad = NULL;	\
	DEBUG_Xv(PerlIO_printf(Perl_debug_log, "Pad set_null\n"));

#define PAD_SAVE_LOCAL(opad,npad) \
	opad = PL_comppad;					\
	PL_comppad = (npad);					\
	PL_curpad =  PL_comppad ? AvARRAY(PL_comppad) : NULL;	\
	DEBUG_Xv(PerlIO_printf(Perl_debug_log,			\
	      "Pad 0x%"UVxf"[0x%"UVxf"] save_local\n",		\
	      PTR2UV(PL_comppad), PTR2UV(PL_curpad)));

#define PAD_RESTORE_LOCAL(opad) \
	PL_comppad = opad;					\
	PL_curpad =  PL_comppad ? AvARRAY(PL_comppad) : NULL;	\
	DEBUG_Xv(PerlIO_printf(Perl_debug_log,			\
	      "Pad 0x%"UVxf"[0x%"UVxf"] restore_local\n",	\
	      PTR2UV(PL_comppad), PTR2UV(PL_curpad)));


/*
=for apidoc m|void|CX_CURPAD_SAVE|struct context
Save the current pad in the given context block structure.

=for apidoc m|SV *|CX_CURPAD_SV|struct context|PADOFFSET po
Access the SV at offset po in the saved current pad in the given
context block structure (can be used as an lvalue).

=cut
*/

#define CX_CURPAD_SAVE(block)  (block).oldcomppad = PL_comppad
#define CX_CURPAD_SV(block,po) (AvARRAY((AV*)((block).oldcomppad))[po])


/*
=for apidoc m|U32|PAD_COMPNAME_FLAGS|PADOFFSET po
Return the flags for the current compiling pad name
at offset C<po>. Assumes a valid slot entry.

=for apidoc m|char *|PAD_COMPNAME_PV|PADOFFSET po
Return the name of the current compiling pad name
at offset C<po>. Assumes a valid slot entry.

=for apidoc m|HV *|PAD_COMPNAME_TYPE|PADOFFSET po
Return the type (stash) of the current compiling pad name at offset
C<po>. Must be a valid name. Returns null if not typed.

=for apidoc m|HV *|PAD_COMPNAME_OURSTASH|PADOFFSET po
Return the stash associated with an C<our> variable.
Assumes the slot entry is a valid C<our> lexical.

=for apidoc m|STRLEN|PAD_COMPNAME_GEN|PADOFFSET po
The generation number of the name at offset C<po> in the current
compiling pad (lvalue). Note that C<SvUVX> is hijacked for this purpose.

=for apidoc m|STRLEN|PAD_COMPNAME_GEN_set|PADOFFSET po|int gen
Sets the generation number of the name at offset C<po> in the current
ling pad (lvalue) to C<gen>.  Note that C<SvUV_set> is hijacked for this purpose.

=cut

*/

#define PAD_COMPNAME_SV(po) (*av_fetch(PL_comppad_name, (po), FALSE))
#define PAD_COMPNAME_FLAGS(po) SvFLAGS(PAD_COMPNAME_SV(po))
#define PAD_COMPNAME_FLAGS_isOUR(po) \
  ((PAD_COMPNAME_FLAGS(po) & (SVpad_NAME|SVpad_OUR)) == (SVpad_NAME|SVpad_OUR))
#define PAD_COMPNAME_PV(po) SvPV_nolen(PAD_COMPNAME_SV(po))

#define PAD_COMPNAME_TYPE(po) pad_compname_type(po)

#define PAD_COMPNAME_OURSTASH(po) \
    (SvOURSTASH(PAD_COMPNAME_SV(po)))

#define PAD_COMPNAME_GEN(po) ((STRLEN)SvUVX(AvARRAY(PL_comppad_name)[po]))

#define PAD_COMPNAME_GEN_set(po, gen) SvUV_set(AvARRAY(PL_comppad_name)[po], (UV)(gen))


/*
=for apidoc m|void|PAD_DUP|PADLIST dstpad|PADLIST srcpad|CLONE_PARAMS* param
Clone a padlist.

=for apidoc m|void|PAD_CLONE_VARS|PerlInterpreter *proto_perl|CLONE_PARAMS* param
Clone the state variables associated with running and compiling pads.

=cut
*/


#define PAD_DUP(dstpad, srcpad, param)				\
    if ((srcpad) && !AvREAL(srcpad)) {				\
	/* XXX padlists are real, but pretend to be not */ 	\
	AvREAL_on(srcpad);					\
	(dstpad) = av_dup_inc((srcpad), param);			\
	AvREAL_off(srcpad);					\
	AvREAL_off(dstpad);					\
    }								\
    else							\
	(dstpad) = av_dup_inc((srcpad), param);			

/* NB - we set PL_comppad to null unless it points at a value that
 * has already been dup'ed, ie it points to part of an active padlist.
 * Otherwise PL_comppad ends up being a leaked scalar in code like
 * the following:
 *     threads->create(sub { threads->create(sub {...} ) } );
 * where the second thread dups the outer sub's comppad but not the
 * sub's CV or padlist. */

#define PAD_CLONE_VARS(proto_perl, param)				\
    PL_comppad = (AV *) ptr_table_fetch(PL_ptr_table, proto_perl->Icomppad); \
    PL_curpad = PL_comppad ?  AvARRAY(PL_comppad) : NULL;		\
    PL_comppad_name		= av_dup(proto_perl->Icomppad_name, param); \
    PL_comppad_name_fill	= proto_perl->Icomppad_name_fill;	\
    PL_comppad_name_floor	= proto_perl->Icomppad_name_floor;	\
    PL_min_intro_pending	= proto_perl->Imin_intro_pending;	\
    PL_max_intro_pending	= proto_perl->Imax_intro_pending;	\
    PL_padix			= proto_perl->Ipadix;			\
    PL_padix_floor		= proto_perl->Ipadix_floor;		\
    PL_pad_reset_pending	= proto_perl->Ipad_reset_pending;	\
    PL_cop_seqmax		= proto_perl->Icop_seqmax;

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
