/*    scope.h
 *
 *    Copyright (C) 1993, 1994, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2004, 2005, 2006, 2007 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#define SAVEt_ITEM		0
#define SAVEt_SV		1
#define SAVEt_AV		2
#define SAVEt_HV		3
#define SAVEt_INT		4
#define SAVEt_LONG		5
#define SAVEt_I32		6
#define SAVEt_IV		7
#define SAVEt_SPTR		8
#define SAVEt_APTR		9
#define SAVEt_HPTR		10
#define SAVEt_PPTR		11
#define SAVEt_NSTAB		12
#define SAVEt_SVREF		13
#define SAVEt_GP		14
#define SAVEt_FREESV		15
#define SAVEt_FREEOP		16
#define SAVEt_FREEPV		17
#define SAVEt_CLEARSV		18
#define SAVEt_DELETE		19
#define SAVEt_DESTRUCTOR	20
#define SAVEt_REGCONTEXT	21
#define SAVEt_STACK_POS		22
#define SAVEt_I16		23
#define SAVEt_AELEM		24
#define SAVEt_HELEM		25
#define SAVEt_OP		26
#define SAVEt_HINTS		27
#define SAVEt_ALLOC		28
#define SAVEt_GENERIC_SVREF	29
#define SAVEt_DESTRUCTOR_X	30
#define SAVEt_VPTR		31
#define SAVEt_I8		32
#define SAVEt_COMPPAD		33
#define SAVEt_GENERIC_PVREF	34
#define SAVEt_PADSV		35
#define SAVEt_MORTALIZESV	36
#define SAVEt_SHARED_PVREF	37
#define SAVEt_BOOL		38
#define SAVEt_SET_SVFLAGS	39
#define SAVEt_SAVESWITCHSTACK	40
#define SAVEt_COP_ARYBASE	41
#define SAVEt_RE_STATE		42
#define SAVEt_COMPILE_WARNINGS	43
#define SAVEt_STACK_CXPOS	44
#define SAVEt_PARSER		45

#ifndef SCOPE_SAVES_SIGNAL_MASK
#define SCOPE_SAVES_SIGNAL_MASK 0
#endif

#define SSCHECK(need) if (PL_savestack_ix + (I32)(need) > PL_savestack_max) savestack_grow()
#define SSGROW(need) if (PL_savestack_ix + (I32)(need) > PL_savestack_max) savestack_grow_cnt(need)
#define SSPUSHINT(i) (PL_savestack[PL_savestack_ix++].any_i32 = (I32)(i))
#define SSPUSHLONG(i) (PL_savestack[PL_savestack_ix++].any_long = (long)(i))
#define SSPUSHBOOL(p) (PL_savestack[PL_savestack_ix++].any_bool = (p))
#define SSPUSHIV(i) (PL_savestack[PL_savestack_ix++].any_iv = (IV)(i))
#define SSPUSHPTR(p) (PL_savestack[PL_savestack_ix++].any_ptr = (void*)(p))
#define SSPUSHDPTR(p) (PL_savestack[PL_savestack_ix++].any_dptr = (p))
#define SSPUSHDXPTR(p) (PL_savestack[PL_savestack_ix++].any_dxptr = (p))
#define SSPOPINT (PL_savestack[--PL_savestack_ix].any_i32)
#define SSPOPLONG (PL_savestack[--PL_savestack_ix].any_long)
#define SSPOPBOOL (PL_savestack[--PL_savestack_ix].any_bool)
#define SSPOPIV (PL_savestack[--PL_savestack_ix].any_iv)
#define SSPOPPTR (PL_savestack[--PL_savestack_ix].any_ptr)
#define SSPOPDPTR (PL_savestack[--PL_savestack_ix].any_dptr)
#define SSPOPDXPTR (PL_savestack[--PL_savestack_ix].any_dxptr)

/*
=head1 Callback Functions

=for apidoc Ams||SAVETMPS
Opening bracket for temporaries on a callback.  See C<FREETMPS> and
L<perlcall>.

=for apidoc Ams||FREETMPS
Closing bracket for temporaries on a callback.  See C<SAVETMPS> and
L<perlcall>.

=for apidoc Ams||ENTER
Opening bracket on a callback.  See C<LEAVE> and L<perlcall>.

=for apidoc Ams||LEAVE
Closing bracket on a callback.  See C<ENTER> and L<perlcall>.

=cut
*/

#define SAVETMPS save_int((int*)&PL_tmps_floor), PL_tmps_floor = PL_tmps_ix
#define FREETMPS if (PL_tmps_ix > PL_tmps_floor) free_tmps()

#ifdef DEBUGGING
#define ENTER							\
    STMT_START {						\
	push_scope();						\
	DEBUG_SCOPE("ENTER")					\
    } STMT_END
#define LEAVE							\
    STMT_START {						\
	DEBUG_SCOPE("LEAVE")					\
	pop_scope();						\
    } STMT_END
#else
#define ENTER push_scope()
#define LEAVE pop_scope()
#endif
#define LEAVE_SCOPE(old) if (PL_savestack_ix > old) leave_scope(old)

#define SAVEI8(i)	save_I8((I8*)&(i))
#define SAVEI16(i)	save_I16((I16*)&(i))
#define SAVEI32(i)	save_I32((I32*)&(i))
#define SAVEINT(i)	save_int((int*)&(i))
#define SAVEIV(i)	save_iv((IV*)&(i))
#define SAVELONG(l)	save_long((long*)&(l))
#define SAVEBOOL(b)	save_bool((bool*)&(b))
#define SAVESPTR(s)	save_sptr((SV**)&(s))
#define SAVEPPTR(s)	save_pptr((char**)&(s))
#define SAVEVPTR(s)	save_vptr((void*)&(s))
#define SAVEPADSV(s)	save_padsv(s)
#define SAVEFREESV(s)	save_freesv((SV*)(s))
#define SAVEMORTALIZESV(s)	save_mortalizesv((SV*)(s))
#define SAVEFREEOP(o)	save_freeop((OP*)(o))
#define SAVEFREEPV(p)	save_freepv((char*)(p))
#define SAVECLEARSV(sv)	save_clearsv((SV**)&(sv))
#define SAVEGENERICSV(s)	save_generic_svref((SV**)&(s))
#define SAVEGENERICPV(s)	save_generic_pvref((char**)&(s))
#define SAVESHAREDPV(s)		save_shared_pvref((char**)&(s))
#define SAVESETSVFLAGS(sv,mask,val)	save_set_svflags(sv,mask,val)
#define SAVEDELETE(h,k,l) \
	  save_delete((HV*)(h), (char*)(k), (I32)(l))
#define SAVEDESTRUCTOR(f,p) \
	  save_destructor((DESTRUCTORFUNC_NOCONTEXT_t)(f), (void*)(p))

#define SAVEDESTRUCTOR_X(f,p) \
	  save_destructor_x((DESTRUCTORFUNC_t)(f), (void*)(p))

#define SAVESTACK_POS() \
    STMT_START {				\
	SSCHECK(2);				\
	SSPUSHINT(PL_stack_sp - PL_stack_base);	\
	SSPUSHINT(SAVEt_STACK_POS);		\
    } STMT_END

#define SAVEOP()	save_op()

#define SAVEHINTS() \
    STMT_START {					\
	SSCHECK(4);					\
	if (PL_hints & HINT_LOCALIZE_HH) {		\
	    SSPUSHPTR(GvHV(PL_hintgv));			\
	    GvHV(PL_hintgv) = Perl_hv_copy_hints_hv(aTHX_ GvHV(PL_hintgv)); \
	}						\
	if (PL_compiling.cop_hints_hash) {		\
	    HINTS_REFCNT_LOCK;				\
	    PL_compiling.cop_hints_hash->refcounted_he_refcnt++;	\
	    HINTS_REFCNT_UNLOCK;			\
	}						\
	SSPUSHPTR(PL_compiling.cop_hints_hash);		\
	SSPUSHINT(PL_hints);				\
	SSPUSHINT(SAVEt_HINTS);				\
    } STMT_END

#define SAVECOMPPAD() \
    STMT_START {						\
	SSCHECK(2);						\
	SSPUSHPTR((SV*)PL_comppad);				\
	SSPUSHINT(SAVEt_COMPPAD);				\
    } STMT_END

#define SAVESWITCHSTACK(f,t) \
    STMT_START {					\
	SSCHECK(3);					\
	SSPUSHPTR((SV*)(f));				\
	SSPUSHPTR((SV*)(t));				\
	SSPUSHINT(SAVEt_SAVESWITCHSTACK);		\
	SWITCHSTACK((f),(t));				\
	PL_curstackinfo->si_stack = (t);		\
    } STMT_END

#define SAVECOPARYBASE(c) \
    STMT_START {					\
	SSCHECK(3);					\
	SSPUSHINT(CopARYBASE_get(c));			\
	SSPUSHPTR(c);					\
	SSPUSHINT(SAVEt_COP_ARYBASE);			\
    } STMT_END

/* Need to do the cop warnings like this, rather than a "SAVEFREESHAREDPV",
   because realloc() means that the value can actually change. Possibly
   could have done savefreesharedpvREF, but this way actually seems cleaner,
   as it simplifies the code that does the saves, and reduces the load on the
   save stack.  */
#define SAVECOMPILEWARNINGS() \
    STMT_START {					\
	SSCHECK(2);					\
	SSPUSHPTR(PL_compiling.cop_warnings);		\
	SSPUSHINT(SAVEt_COMPILE_WARNINGS);		\
    } STMT_END

#define SAVESTACK_CXPOS() \
    STMT_START {                                  \
        SSCHECK(3);                               \
        SSPUSHINT(cxstack[cxstack_ix].blk_oldsp); \
        SSPUSHINT(cxstack_ix);                    \
        SSPUSHINT(SAVEt_STACK_CXPOS);             \
    } STMT_END

#define SAVEPARSER(p) \
    STMT_START {                                  \
        SSCHECK(2);                               \
        SSPUSHPTR(p);		                  \
        SSPUSHINT(SAVEt_PARSER); 	          \
    } STMT_END

#ifdef USE_ITHREADS
#  define SAVECOPSTASH(c)	SAVEPPTR(CopSTASHPV(c))
#  define SAVECOPSTASH_FREE(c)	SAVESHAREDPV(CopSTASHPV(c))
#  define SAVECOPFILE(c)	SAVEPPTR(CopFILE(c))
#  define SAVECOPFILE_FREE(c)	SAVESHAREDPV(CopFILE(c))
#  define SAVECOPLABEL(c)	SAVEPPTR(CopLABEL(c))
#  define SAVECOPLABEL_FREE(c)	SAVESHAREDPV(CopLABEL(c))
#else
#  define SAVECOPSTASH(c)	SAVESPTR(CopSTASH(c))
#  define SAVECOPSTASH_FREE(c)	SAVECOPSTASH(c)	/* XXX not refcounted */
#  define SAVECOPFILE(c)	SAVESPTR(CopFILEGV(c))
#  define SAVECOPFILE_FREE(c)	SAVEGENERICSV(CopFILEGV(c))
#  define SAVECOPLABEL(c)	SAVEPPTR(CopLABEL(c))
#  define SAVECOPLABEL_FREE(c)	SAVEPPTR(CopLABEL(c))
#endif

#define SAVECOPLINE(c)		SAVEI32(CopLINE(c))

/* SSNEW() temporarily allocates a specified number of bytes of data on the
 * savestack.  It returns an integer index into the savestack, because a
 * pointer would get broken if the savestack is moved on reallocation.
 * SSNEWa() works like SSNEW(), but also aligns the data to the specified
 * number of bytes.  MEM_ALIGNBYTES is perhaps the most useful.  The
 * alignment will be preserved therough savestack reallocation *only* if
 * realloc returns data aligned to a size divisible by "align"!
 *
 * SSPTR() converts the index returned by SSNEW/SSNEWa() into a pointer.
 */

#define SSNEW(size)             Perl_save_alloc(aTHX_ (size), 0)
#define SSNEWt(n,t)             SSNEW((n)*sizeof(t))
#define SSNEWa(size,align)	Perl_save_alloc(aTHX_ (size), \
    (align - ((int)((caddr_t)&PL_savestack[PL_savestack_ix]) % align)) % align)
#define SSNEWat(n,t,align)	SSNEWa((n)*sizeof(t), align)

#define SSPTR(off,type)         ((type)  ((char*)PL_savestack + off))
#define SSPTRt(off,type)        ((type*) ((char*)PL_savestack + off))

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
