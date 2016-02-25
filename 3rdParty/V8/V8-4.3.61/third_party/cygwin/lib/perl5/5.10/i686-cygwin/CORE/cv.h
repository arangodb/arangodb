/*    cv.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This structure must the beginning of XPVFM in sv.h  */

struct xpvcv {
    union {
	NV	xnv_nv;		/* numeric value, if any */
	HV *	xgv_stash;
	struct {
	    U32	xlow;
	    U32	xhigh;
	}	xpad_cop_seq;	/* used by pad.c for cop_sequence */
	struct {
	    U32 xbm_previous;	/* how many characters in string before rare? */
	    U8	xbm_flags;
	    U8	xbm_rare;	/* rarest character in string */
	}	xbm_s;		/* fields from PVBM */
    }		xnv_u;
    STRLEN	xpv_cur;	/* length of xp_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    union {
	IV	xivu_iv;
	UV	xivu_uv;
	void *	xivu_p1;
	I32	xivu_i32;	/* depth, >= 2 indicates recursive call */
	HEK *	xivu_namehek;
    }		xiv_u;
    union {
	MAGIC*	xmg_magic;	/* linked list of magicalness */
	HV*	xmg_ourstash;	/* Stash for our (when SvPAD_OUR is true) */
    } xmg_u;
    HV*		xmg_stash;	/* class package */

    HV *	xcv_stash;
    union {
	OP *	xcv_start;
	ANY	xcv_xsubany;
    }		xcv_start_u;
    union {
	OP *	xcv_root;
	void	(*xcv_xsub) (pTHX_ CV*);
    }		xcv_root_u;
    GV *	xcv_gv;
    char *	xcv_file;
    PADLIST *	xcv_padlist;
    CV *	xcv_outside;
    U32		xcv_outside_seq; /* the COP sequence (at the point of our
				  * compilation) in the lexically enclosing
				  * sub */
    cv_flags_t	xcv_flags;
};

typedef struct {
    STRLEN	xpv_cur;	/* length of xp_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    union {
	IV	xivu_iv;
	UV	xivu_uv;
	void *	xivu_p1;
	I32	xivu_i32;	/* depth, >= 2 indicates recursive call */
	HEK *	xivu_namehek;
    }		xiv_u;
    union {
	MAGIC*	xmg_magic;	/* linked list of magicalness */
	HV*	xmg_ourstash;	/* Stash for our (when SvPAD_OUR is true) */
    } xmg_u;
    HV*		xmg_stash;	/* class package */

    HV *	xcv_stash;
    union {
	OP *	xcv_start;
	ANY	xcv_xsubany;
    }		xcv_start_u;
    union {
	OP *	xcv_root;
	void	(*xcv_xsub) (pTHX_ CV*);
    }		xcv_root_u;
    GV *	xcv_gv;
    char *	xcv_file;
    PADLIST *	xcv_padlist;
    CV *	xcv_outside;
    U32		xcv_outside_seq; /* the COP sequence (at the point of our
				  * compilation) in the lexically enclosing
				  * sub */
    cv_flags_t	xcv_flags;
} xpvcv_allocated;

/*
=head1 Handy Values

=for apidoc AmU||Nullcv
Null CV pointer.

=head1 CV Manipulation Functions

=for apidoc Am|HV*|CvSTASH|CV* cv
Returns the stash of the CV.

=cut
*/

#define Nullcv Null(CV*)

#define CvSTASH(sv)	((XPVCV*)SvANY(sv))->xcv_stash
#define CvSTART(sv)	((XPVCV*)SvANY(sv))->xcv_start_u.xcv_start
#define CvROOT(sv)	((XPVCV*)SvANY(sv))->xcv_root_u.xcv_root
#define CvXSUB(sv)	((XPVCV*)SvANY(sv))->xcv_root_u.xcv_xsub
#define CvXSUBANY(sv)	((XPVCV*)SvANY(sv))->xcv_start_u.xcv_xsubany
#define CvGV(sv)	((XPVCV*)SvANY(sv))->xcv_gv
#define CvFILE(sv)	((XPVCV*)SvANY(sv))->xcv_file
#ifdef USE_ITHREADS
#  define CvFILE_set_from_cop(sv, cop)	(CvFILE(sv) = savepv(CopFILE(cop)))
#else
#  define CvFILE_set_from_cop(sv, cop)	(CvFILE(sv) = CopFILE(cop))
#endif
#define CvFILEGV(sv)	(gv_fetchfile(CvFILE(sv)))
#if defined(__GNUC__) && !defined(PERL_GCC_BRACE_GROUPS_FORBIDDEN)
#  define CvDEPTH(sv) (*({const CV *_cv = (CV *)sv; \
			  assert(SvTYPE(_cv) == SVt_PVCV ||	 \
				 SvTYPE(_cv) == SVt_PVFM);	 \
			  &((XPVCV*)SvANY(_cv))->xiv_u.xivu_i32; \
			}))
#else
#  define CvDEPTH(sv)	((XPVCV*)SvANY(sv))->xiv_u.xivu_i32
#endif
#define CvPADLIST(sv)	((XPVCV*)SvANY(sv))->xcv_padlist
#define CvOUTSIDE(sv)	((XPVCV*)SvANY(sv))->xcv_outside
#define CvFLAGS(sv)	((XPVCV*)SvANY(sv))->xcv_flags
#define CvOUTSIDE_SEQ(sv) ((XPVCV*)SvANY(sv))->xcv_outside_seq

#define CVf_METHOD	0x0001	/* CV is explicitly marked as a method */
#define CVf_LOCKED	0x0002	/* CV locks itself or first arg on entry */
#define CVf_LVALUE	0x0004  /* CV return value can be used as lvalue */

#define CVf_WEAKOUTSIDE	0x0010  /* CvOUTSIDE isn't ref counted */
#define CVf_CLONE	0x0020	/* anon CV uses external lexicals */
#define CVf_CLONED	0x0040	/* a clone of one of those */
#define CVf_ANON	0x0080	/* CvGV() can't be trusted */
#define CVf_UNIQUE	0x0100	/* sub is only called once (eg PL_main_cv,
				 * require, eval). */
#define CVf_NODEBUG	0x0200	/* no DB::sub indirection for this CV
				   (esp. useful for special XSUBs) */
#define CVf_CONST	0x0400  /* inlinable sub */
#define CVf_ISXSUB	0x0800	/* CV is an XSUB, not pure perl.  */

/* This symbol for optimised communication between toke.c and op.c: */
#define CVf_BUILTIN_ATTRS	(CVf_METHOD|CVf_LOCKED|CVf_LVALUE)

#define CvCLONE(cv)		(CvFLAGS(cv) & CVf_CLONE)
#define CvCLONE_on(cv)		(CvFLAGS(cv) |= CVf_CLONE)
#define CvCLONE_off(cv)		(CvFLAGS(cv) &= ~CVf_CLONE)

#define CvCLONED(cv)		(CvFLAGS(cv) & CVf_CLONED)
#define CvCLONED_on(cv)		(CvFLAGS(cv) |= CVf_CLONED)
#define CvCLONED_off(cv)	(CvFLAGS(cv) &= ~CVf_CLONED)

#define CvANON(cv)		(CvFLAGS(cv) & CVf_ANON)
#define CvANON_on(cv)		(CvFLAGS(cv) |= CVf_ANON)
#define CvANON_off(cv)		(CvFLAGS(cv) &= ~CVf_ANON)

#define CvUNIQUE(cv)		(CvFLAGS(cv) & CVf_UNIQUE)
#define CvUNIQUE_on(cv)		(CvFLAGS(cv) |= CVf_UNIQUE)
#define CvUNIQUE_off(cv)	(CvFLAGS(cv) &= ~CVf_UNIQUE)

#define CvNODEBUG(cv)		(CvFLAGS(cv) & CVf_NODEBUG)
#define CvNODEBUG_on(cv)	(CvFLAGS(cv) |= CVf_NODEBUG)
#define CvNODEBUG_off(cv)	(CvFLAGS(cv) &= ~CVf_NODEBUG)

#define CvMETHOD(cv)		(CvFLAGS(cv) & CVf_METHOD)
#define CvMETHOD_on(cv)		(CvFLAGS(cv) |= CVf_METHOD)
#define CvMETHOD_off(cv)	(CvFLAGS(cv) &= ~CVf_METHOD)

#define CvLOCKED(cv)		(CvFLAGS(cv) & CVf_LOCKED)
#define CvLOCKED_on(cv)		(CvFLAGS(cv) |= CVf_LOCKED)
#define CvLOCKED_off(cv)	(CvFLAGS(cv) &= ~CVf_LOCKED)

#define CvLVALUE(cv)		(CvFLAGS(cv) & CVf_LVALUE)
#define CvLVALUE_on(cv)		(CvFLAGS(cv) |= CVf_LVALUE)
#define CvLVALUE_off(cv)	(CvFLAGS(cv) &= ~CVf_LVALUE)

#define CvEVAL(cv)		(CvUNIQUE(cv) && !SvFAKE(cv))
#define CvEVAL_on(cv)		(CvUNIQUE_on(cv),SvFAKE_off(cv))
#define CvEVAL_off(cv)		CvUNIQUE_off(cv)

/* BEGIN|CHECK|INIT|UNITCHECK|END */
#define CvSPECIAL(cv)		(CvUNIQUE(cv) && SvFAKE(cv))
#define CvSPECIAL_on(cv)	(CvUNIQUE_on(cv),SvFAKE_on(cv))
#define CvSPECIAL_off(cv)	(CvUNIQUE_off(cv),SvFAKE_off(cv))

#define CvCONST(cv)		(CvFLAGS(cv) & CVf_CONST)
#define CvCONST_on(cv)		(CvFLAGS(cv) |= CVf_CONST)
#define CvCONST_off(cv)		(CvFLAGS(cv) &= ~CVf_CONST)

#define CvWEAKOUTSIDE(cv)	(CvFLAGS(cv) & CVf_WEAKOUTSIDE)
#define CvWEAKOUTSIDE_on(cv)	(CvFLAGS(cv) |= CVf_WEAKOUTSIDE)
#define CvWEAKOUTSIDE_off(cv)	(CvFLAGS(cv) &= ~CVf_WEAKOUTSIDE)

#define CvISXSUB(cv)		(CvFLAGS(cv) & CVf_ISXSUB)
#define CvISXSUB_on(cv)		(CvFLAGS(cv) |= CVf_ISXSUB)
#define CvISXSUB_off(cv)	(CvFLAGS(cv) &= ~CVf_ISXSUB)

/* Flags for newXS_flags  */
#define XS_DYNAMIC_FILENAME	0x01	/* The filename isn't static  */

/*
=head1 CV reference counts and CvOUTSIDE

=for apidoc m|bool|CvWEAKOUTSIDE|CV *cv

Each CV has a pointer, C<CvOUTSIDE()>, to its lexically enclosing
CV (if any). Because pointers to anonymous sub prototypes are
stored in C<&> pad slots, it is a possible to get a circular reference,
with the parent pointing to the child and vice-versa. To avoid the
ensuing memory leak, we do not increment the reference count of the CV
pointed to by C<CvOUTSIDE> in the I<one specific instance> that the parent
has a C<&> pad slot pointing back to us. In this case, we set the
C<CvWEAKOUTSIDE> flag in the child. This allows us to determine under what
circumstances we should decrement the refcount of the parent when freeing
the child.

There is a further complication with non-closure anonymous subs (i.e. those
that do not refer to any lexicals outside that sub). In this case, the
anonymous prototype is shared rather than being cloned. This has the
consequence that the parent may be freed while there are still active
children, eg

    BEGIN { $a = sub { eval '$x' } }

In this case, the BEGIN is freed immediately after execution since there
are no active references to it: the anon sub prototype has
C<CvWEAKOUTSIDE> set since it's not a closure, and $a points to the same
CV, so it doesn't contribute to BEGIN's refcount either.  When $a is
executed, the C<eval '$x'> causes the chain of C<CvOUTSIDE>s to be followed,
and the freed BEGIN is accessed.

To avoid this, whenever a CV and its associated pad is freed, any
C<&> entries in the pad are explicitly removed from the pad, and if the
refcount of the pointed-to anon sub is still positive, then that
child's C<CvOUTSIDE> is set to point to its grandparent. This will only
occur in the single specific case of a non-closure anon prototype
having one or more active references (such as C<$a> above).

One other thing to consider is that a CV may be merely undefined
rather than freed, eg C<undef &foo>. In this case, its refcount may
not have reached zero, but we still delete its pad and its C<CvROOT> etc.
Since various children may still have their C<CvOUTSIDE> pointing at this
undefined CV, we keep its own C<CvOUTSIDE> for the time being, so that
the chain of lexical scopes is unbroken. For example, the following
should print 123:

    my $x = 123;
    sub tmp { sub { eval '$x' } }
    my $a = tmp();
    undef &tmp;
    print  $a->();

=cut
*/

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
