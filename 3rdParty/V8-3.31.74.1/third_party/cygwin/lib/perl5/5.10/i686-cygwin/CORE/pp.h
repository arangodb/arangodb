/*    pp.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#define PP(s) OP * Perl_##s(pTHX)

/*
=head1 Stack Manipulation Macros

=for apidoc AmU||SP
Stack pointer.  This is usually handled by C<xsubpp>.  See C<dSP> and
C<SPAGAIN>.

=for apidoc AmU||MARK
Stack marker variable for the XSUB.  See C<dMARK>.

=for apidoc Am|void|PUSHMARK|SP
Opening bracket for arguments on a callback.  See C<PUTBACK> and
L<perlcall>.

=for apidoc Ams||dSP
Declares a local copy of perl's stack pointer for the XSUB, available via
the C<SP> macro.  See C<SP>.

=for apidoc ms||djSP

Declare Just C<SP>. This is actually identical to C<dSP>, and declares
a local copy of perl's stack pointer, available via the C<SP> macro.
See C<SP>.  (Available for backward source code compatibility with the
old (Perl 5.005) thread model.)

=for apidoc Ams||dMARK
Declare a stack marker variable, C<mark>, for the XSUB.  See C<MARK> and
C<dORIGMARK>.

=for apidoc Ams||dORIGMARK
Saves the original stack mark for the XSUB.  See C<ORIGMARK>.

=for apidoc AmU||ORIGMARK
The original stack mark for the XSUB.  See C<dORIGMARK>.

=for apidoc Ams||SPAGAIN
Refetch the stack pointer.  Used after a callback.  See L<perlcall>.

=cut */

#undef SP /* Solaris 2.7 i386 has this in /usr/include/sys/reg.h */
#define SP sp
#define MARK mark
#define TARG targ

#define PUSHMARK(p)	\
	STMT_START {					\
	    if (++PL_markstack_ptr == PL_markstack_max)	\
	    markstack_grow();				\
	    *PL_markstack_ptr = (I32)((p) - PL_stack_base);\
	} STMT_END

#define TOPMARK		(*PL_markstack_ptr)
#define POPMARK		(*PL_markstack_ptr--)

#define dSP		SV **sp = PL_stack_sp
#define djSP		dSP
#define dMARK		register SV **mark = PL_stack_base + POPMARK
#define dORIGMARK	const I32 origmark = (I32)(mark - PL_stack_base)
#define ORIGMARK	(PL_stack_base + origmark)

#define SPAGAIN		sp = PL_stack_sp
#define MSPAGAIN	STMT_START { sp = PL_stack_sp; mark = ORIGMARK; } STMT_END

#define GETTARGETSTACKED targ = (PL_op->op_flags & OPf_STACKED ? POPs : PAD_SV(PL_op->op_targ))
#define dTARGETSTACKED SV * GETTARGETSTACKED

#define GETTARGET targ = PAD_SV(PL_op->op_targ)
#define dTARGET SV * GETTARGET

#define GETATARGET targ = (PL_op->op_flags & OPf_STACKED ? sp[-1] : PAD_SV(PL_op->op_targ))
#define dATARGET SV * GETATARGET

#define dTARG SV *targ

#define NORMAL PL_op->op_next
#define DIE return Perl_die

/*
=for apidoc Ams||PUTBACK
Closing bracket for XSUB arguments.  This is usually handled by C<xsubpp>.
See C<PUSHMARK> and L<perlcall> for other uses.

=for apidoc Amn|SV*|POPs
Pops an SV off the stack.

=for apidoc Amn|char*|POPp
Pops a string off the stack. Deprecated. New code should use POPpx.

=for apidoc Amn|char*|POPpx
Pops a string off the stack.

=for apidoc Amn|char*|POPpbytex
Pops a string off the stack which must consist of bytes i.e. characters < 256.

=for apidoc Amn|NV|POPn
Pops a double off the stack.

=for apidoc Amn|IV|POPi
Pops an integer off the stack.

=for apidoc Amn|long|POPl
Pops a long off the stack.

=cut
*/

#define PUTBACK		PL_stack_sp = sp
#define RETURN		return (PUTBACK, NORMAL)
#define RETURNOP(o)	return (PUTBACK, o)
#define RETURNX(x)	return (x, PUTBACK, NORMAL)

#define POPs		(*sp--)
#define POPp		(SvPVx(POPs, PL_na))		/* deprecated */
#define POPpx		(SvPVx_nolen(POPs))
#define POPpconstx	(SvPVx_nolen_const(POPs))
#define POPpbytex	(SvPVbytex_nolen(POPs))
#define POPn		(SvNVx(POPs))
#define POPi		((IV)SvIVx(POPs))
#define POPu		((UV)SvUVx(POPs))
#define POPl		((long)SvIVx(POPs))
#define POPul		((unsigned long)SvIVx(POPs))
#ifdef HAS_QUAD
#define POPq		((Quad_t)SvIVx(POPs))
#define POPuq		((Uquad_t)SvUVx(POPs))
#endif

#define TOPs		(*sp)
#define TOPm1s		(*(sp-1))
#define TOPp1s		(*(sp+1))
#define TOPp		(SvPV(TOPs, PL_na))		/* deprecated */
#define TOPpx		(SvPV_nolen(TOPs))
#define TOPn		(SvNV(TOPs))
#define TOPi		((IV)SvIV(TOPs))
#define TOPu		((UV)SvUV(TOPs))
#define TOPl		((long)SvIV(TOPs))
#define TOPul		((unsigned long)SvUV(TOPs))
#ifdef HAS_QUAD
#define TOPq		((Quad_t)SvIV(TOPs))
#define TOPuq		((Uquad_t)SvUV(TOPs))
#endif

/* Go to some pains in the rare event that we must extend the stack. */

/*
=for apidoc Am|void|EXTEND|SP|int nitems
Used to extend the argument stack for an XSUB's return values. Once
used, guarantees that there is room for at least C<nitems> to be pushed
onto the stack.

=for apidoc Am|void|PUSHs|SV* sv
Push an SV onto the stack.  The stack must have room for this element.
Does not handle 'set' magic.  Does not use C<TARG>.  See also C<PUSHmortal>,
C<XPUSHs> and C<XPUSHmortal>.

=for apidoc Am|void|PUSHp|char* str|STRLEN len
Push a string onto the stack.  The stack must have room for this element.
The C<len> indicates the length of the string.  Handles 'set' magic.  Uses
C<TARG>, so C<dTARGET> or C<dXSTARG> should be called to declare it.  Do not
call multiple C<TARG>-oriented macros to return lists from XSUB's - see
C<mPUSHp> instead.  See also C<XPUSHp> and C<mXPUSHp>.

=for apidoc Am|void|PUSHn|NV nv
Push a double onto the stack.  The stack must have room for this element.
Handles 'set' magic.  Uses C<TARG>, so C<dTARGET> or C<dXSTARG> should be
called to declare it.  Do not call multiple C<TARG>-oriented macros to
return lists from XSUB's - see C<mPUSHn> instead.  See also C<XPUSHn> and
C<mXPUSHn>.

=for apidoc Am|void|PUSHi|IV iv
Push an integer onto the stack.  The stack must have room for this element.
Handles 'set' magic.  Uses C<TARG>, so C<dTARGET> or C<dXSTARG> should be
called to declare it.  Do not call multiple C<TARG>-oriented macros to 
return lists from XSUB's - see C<mPUSHi> instead.  See also C<XPUSHi> and
C<mXPUSHi>.

=for apidoc Am|void|PUSHu|UV uv
Push an unsigned integer onto the stack.  The stack must have room for this
element.  Handles 'set' magic.  Uses C<TARG>, so C<dTARGET> or C<dXSTARG>
should be called to declare it.  Do not call multiple C<TARG>-oriented
macros to return lists from XSUB's - see C<mPUSHu> instead.  See also
C<XPUSHu> and C<mXPUSHu>.

=for apidoc Am|void|XPUSHs|SV* sv
Push an SV onto the stack, extending the stack if necessary.  Does not
handle 'set' magic.  Does not use C<TARG>.  See also C<XPUSHmortal>,
C<PUSHs> and C<PUSHmortal>.

=for apidoc Am|void|XPUSHp|char* str|STRLEN len
Push a string onto the stack, extending the stack if necessary.  The C<len>
indicates the length of the string.  Handles 'set' magic.  Uses C<TARG>, so
C<dTARGET> or C<dXSTARG> should be called to declare it.  Do not call
multiple C<TARG>-oriented macros to return lists from XSUB's - see
C<mXPUSHp> instead.  See also C<PUSHp> and C<mPUSHp>.

=for apidoc Am|void|XPUSHn|NV nv
Push a double onto the stack, extending the stack if necessary.  Handles
'set' magic.  Uses C<TARG>, so C<dTARGET> or C<dXSTARG> should be called to
declare it.  Do not call multiple C<TARG>-oriented macros to return lists
from XSUB's - see C<mXPUSHn> instead.  See also C<PUSHn> and C<mPUSHn>.

=for apidoc Am|void|XPUSHi|IV iv
Push an integer onto the stack, extending the stack if necessary.  Handles
'set' magic.  Uses C<TARG>, so C<dTARGET> or C<dXSTARG> should be called to
declare it.  Do not call multiple C<TARG>-oriented macros to return lists
from XSUB's - see C<mXPUSHi> instead.  See also C<PUSHi> and C<mPUSHi>.

=for apidoc Am|void|XPUSHu|UV uv
Push an unsigned integer onto the stack, extending the stack if necessary.
Handles 'set' magic.  Uses C<TARG>, so C<dTARGET> or C<dXSTARG> should be
called to declare it.  Do not call multiple C<TARG>-oriented macros to
return lists from XSUB's - see C<mXPUSHu> instead.  See also C<PUSHu> and
C<mPUSHu>.

=for apidoc Am|void|mPUSHs|SV* sv
Push an SV onto the stack and mortalizes the SV.  The stack must have room
for this element.  Does not use C<TARG>.  See also C<PUSHs> and C<mXPUSHs>.

=for apidoc Am|void|PUSHmortal
Push a new mortal SV onto the stack.  The stack must have room for this
element.  Does not use C<TARG>.  See also C<PUSHs>, C<XPUSHmortal> and C<XPUSHs>.

=for apidoc Am|void|mPUSHp|char* str|STRLEN len
Push a string onto the stack.  The stack must have room for this element.
The C<len> indicates the length of the string.  Does not use C<TARG>.
See also C<PUSHp>, C<mXPUSHp> and C<XPUSHp>.

=for apidoc Am|void|mPUSHn|NV nv
Push a double onto the stack.  The stack must have room for this element.
Does not use C<TARG>.  See also C<PUSHn>, C<mXPUSHn> and C<XPUSHn>.

=for apidoc Am|void|mPUSHi|IV iv
Push an integer onto the stack.  The stack must have room for this element.
Does not use C<TARG>.  See also C<PUSHi>, C<mXPUSHi> and C<XPUSHi>.

=for apidoc Am|void|mPUSHu|UV uv
Push an unsigned integer onto the stack.  The stack must have room for this
element.  Does not use C<TARG>.  See also C<PUSHu>, C<mXPUSHu> and C<XPUSHu>.

=for apidoc Am|void|mXPUSHs|SV* sv
Push an SV onto the stack, extending the stack if necessary and mortalizes
the SV.  Does not use C<TARG>.  See also C<XPUSHs> and C<mPUSHs>.

=for apidoc Am|void|XPUSHmortal
Push a new mortal SV onto the stack, extending the stack if necessary.
Does not use C<TARG>.  See also C<XPUSHs>, C<PUSHmortal> and C<PUSHs>.

=for apidoc Am|void|mXPUSHp|char* str|STRLEN len
Push a string onto the stack, extending the stack if necessary.  The C<len>
indicates the length of the string.  Does not use C<TARG>.  See also C<XPUSHp>,
C<mPUSHp> and C<PUSHp>.

=for apidoc Am|void|mXPUSHn|NV nv
Push a double onto the stack, extending the stack if necessary.
Does not use C<TARG>.  See also C<XPUSHn>, C<mPUSHn> and C<PUSHn>.

=for apidoc Am|void|mXPUSHi|IV iv
Push an integer onto the stack, extending the stack if necessary.
Does not use C<TARG>.  See also C<XPUSHi>, C<mPUSHi> and C<PUSHi>.

=for apidoc Am|void|mXPUSHu|UV uv
Push an unsigned integer onto the stack, extending the stack if necessary.
Does not use C<TARG>.  See also C<XPUSHu>, C<mPUSHu> and C<PUSHu>.

=cut
*/

#define EXTEND(p,n)	STMT_START { if (PL_stack_max - p < (int)(n)) {		\
			    sp = stack_grow(sp,p, (int) (n));		\
			} } STMT_END

/* Same thing, but update mark register too. */
#define MEXTEND(p,n)	STMT_START {if (PL_stack_max - p < (int)(n)) {	\
			    const int markoff = mark - PL_stack_base;	\
			    sp = stack_grow(sp,p,(int) (n));		\
			    mark = PL_stack_base + markoff;		\
			} } STMT_END

#define PUSHs(s)	(*++sp = (s))
#define PUSHTARG	STMT_START { SvSETMAGIC(TARG); PUSHs(TARG); } STMT_END
#define PUSHp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); PUSHTARG; } STMT_END
#define PUSHn(n)	STMT_START { sv_setnv(TARG, (NV)(n)); PUSHTARG; } STMT_END
#define PUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); PUSHTARG; } STMT_END
#define PUSHu(u)	STMT_START { sv_setuv(TARG, (UV)(u)); PUSHTARG; } STMT_END

#define XPUSHs(s)	STMT_START { EXTEND(sp,1); (*++sp = (s)); } STMT_END
#define XPUSHTARG	STMT_START { SvSETMAGIC(TARG); XPUSHs(TARG); } STMT_END
#define XPUSHp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); XPUSHTARG; } STMT_END
#define XPUSHn(n)	STMT_START { sv_setnv(TARG, (NV)(n)); XPUSHTARG; } STMT_END
#define XPUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); XPUSHTARG; } STMT_END
#define XPUSHu(u)	STMT_START { sv_setuv(TARG, (UV)(u)); XPUSHTARG; } STMT_END
#define XPUSHundef	STMT_START { SvOK_off(TARG); XPUSHs(TARG); } STMT_END

#define mPUSHs(s)	PUSHs(sv_2mortal(s))
#define PUSHmortal	PUSHs(sv_newmortal())
#define mPUSHp(p,l)	PUSHs(newSVpvn_flags((p), (l), SVs_TEMP))
#define mPUSHn(n)	sv_setnv(PUSHmortal, (NV)(n))
#define mPUSHi(i)	sv_setiv(PUSHmortal, (IV)(i))
#define mPUSHu(u)	sv_setuv(PUSHmortal, (UV)(u))

#define mXPUSHs(s)	XPUSHs(sv_2mortal(s))
#define XPUSHmortal	XPUSHs(sv_newmortal())
#define mXPUSHp(p,l)	STMT_START { EXTEND(sp,1); mPUSHp((p), (l)); } STMT_END
#define mXPUSHn(n)	STMT_START { EXTEND(sp,1); sv_setnv(PUSHmortal, (NV)(n)); } STMT_END
#define mXPUSHi(i)	STMT_START { EXTEND(sp,1); sv_setiv(PUSHmortal, (IV)(i)); } STMT_END
#define mXPUSHu(u)	STMT_START { EXTEND(sp,1); sv_setuv(PUSHmortal, (UV)(u)); } STMT_END

#define SETs(s)		(*sp = s)
#define SETTARG		STMT_START { SvSETMAGIC(TARG); SETs(TARG); } STMT_END
#define SETp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); SETTARG; } STMT_END
#define SETn(n)		STMT_START { sv_setnv(TARG, (NV)(n)); SETTARG; } STMT_END
#define SETi(i)		STMT_START { sv_setiv(TARG, (IV)(i)); SETTARG; } STMT_END
#define SETu(u)		STMT_START { sv_setuv(TARG, (UV)(u)); SETTARG; } STMT_END

#define dTOPss		SV *sv = TOPs
#define dPOPss		SV *sv = POPs
#define dTOPnv		NV value = TOPn
#define dPOPnv		NV value = POPn
#define dTOPiv		IV value = TOPi
#define dPOPiv		IV value = POPi
#define dTOPuv		UV value = TOPu
#define dPOPuv		UV value = POPu
#ifdef HAS_QUAD
#define dTOPqv		Quad_t value = TOPu
#define dPOPqv		Quad_t value = POPu
#define dTOPuqv		Uquad_t value = TOPuq
#define dPOPuqv		Uquad_t value = POPuq
#endif

#define dPOPXssrl(X)	SV *right = POPs; SV *left = CAT2(X,s)
#define dPOPXnnrl(X)	NV right = POPn; NV left = CAT2(X,n)
#define dPOPXiirl(X)	IV right = POPi; IV left = CAT2(X,i)

#define USE_LEFT(sv) \
	(SvOK(sv) || SvGMAGICAL(sv) || !(PL_op->op_flags & OPf_STACKED))
#define dPOPXnnrl_ul(X)	\
    NV right = POPn;				\
    SV *leftsv = CAT2(X,s);				\
    NV left = USE_LEFT(leftsv) ? SvNV(leftsv) : 0.0
#define dPOPXiirl_ul(X) \
    IV right = POPi;					\
    SV *leftsv = CAT2(X,s);				\
    IV left = USE_LEFT(leftsv) ? SvIV(leftsv) : 0

#define dPOPPOPssrl	dPOPXssrl(POP)
#define dPOPPOPnnrl	dPOPXnnrl(POP)
#define dPOPPOPnnrl_ul	dPOPXnnrl_ul(POP)
#define dPOPPOPiirl	dPOPXiirl(POP)
#define dPOPPOPiirl_ul	dPOPXiirl_ul(POP)

#define dPOPTOPssrl	dPOPXssrl(TOP)
#define dPOPTOPnnrl	dPOPXnnrl(TOP)
#define dPOPTOPnnrl_ul	dPOPXnnrl_ul(TOP)
#define dPOPTOPiirl	dPOPXiirl(TOP)
#define dPOPTOPiirl_ul	dPOPXiirl_ul(TOP)

#define RETPUSHYES	RETURNX(PUSHs(&PL_sv_yes))
#define RETPUSHNO	RETURNX(PUSHs(&PL_sv_no))
#define RETPUSHUNDEF	RETURNX(PUSHs(&PL_sv_undef))

#define RETSETYES	RETURNX(SETs(&PL_sv_yes))
#define RETSETNO	RETURNX(SETs(&PL_sv_no))
#define RETSETUNDEF	RETURNX(SETs(&PL_sv_undef))

#define ARGTARG		PL_op->op_targ

    /* See OPpTARGET_MY: */
#define MAXARG		(PL_op->op_private & 15)

#define SWITCHSTACK(f,t) \
    STMT_START {							\
	AvFILLp(f) = sp - PL_stack_base;				\
	PL_stack_base = AvARRAY(t);					\
	PL_stack_max = PL_stack_base + AvMAX(t);			\
	sp = PL_stack_sp = PL_stack_base + AvFILLp(t);			\
	PL_curstack = t;						\
    } STMT_END

#define EXTEND_MORTAL(n) \
    STMT_START {							\
	if (PL_tmps_ix + (n) >= PL_tmps_max)				\
	    tmps_grow(n);						\
    } STMT_END

#define AMGf_noright	1
#define AMGf_noleft	2
#define AMGf_assign	4
#define AMGf_unary	8

#define tryAMAGICbinW_var(meth_enum,assign,set) STMT_START { \
	    SV* const left = *(sp-1); \
	    SV* const right = *(sp); \
	    if ((SvAMAGIC(left)||SvAMAGIC(right))) {\
		SV * const tmpsv = amagic_call(left, \
				   right, \
				   (meth_enum), \
				   (assign)? AMGf_assign: 0); \
		if (tmpsv) { \
		    SPAGAIN; \
		    (void)POPs; set(tmpsv); RETURN; } \
		} \
	} STMT_END

#define tryAMAGICbinW(meth,assign,set) \
    tryAMAGICbinW_var(CAT2(meth,_amg),assign,set)

#define tryAMAGICbin_var(meth_enum,assign) \
		tryAMAGICbinW_var(meth_enum,assign,SETsv)
#define tryAMAGICbin(meth,assign) \
		tryAMAGICbin_var(CAT2(meth,_amg),assign)

#define tryAMAGICbinSET(meth,assign) tryAMAGICbinW(meth,assign,SETs)

#define tryAMAGICbinSET_var(meth_enum,assign) \
    tryAMAGICbinW_var(meth_enum,assign,SETs)

#define AMG_CALLun_var(sv,meth_enum) amagic_call(sv,&PL_sv_undef,  \
					meth_enum,AMGf_noright | AMGf_unary)
#define AMG_CALLun(sv,meth) AMG_CALLun_var(sv,CAT2(meth,_amg))

#define AMG_CALLbinL(left,right,meth) \
            amagic_call(left,right,CAT2(meth,_amg),AMGf_noright)

#define tryAMAGICunW_var(meth_enum,set,shift,ret) STMT_START { \
	    SV* tmpsv; \
	    SV* arg= sp[shift]; \
          if(0) goto am_again;  /* shut up unused warning */ \
	  am_again: \
	    if ((SvAMAGIC(arg))&&\
		(tmpsv=AMG_CALLun_var(arg,(meth_enum)))) {\
	       SPAGAIN; if (shift) sp += shift; \
	       set(tmpsv); ret; } \
	} STMT_END
#define tryAMAGICunW(meth,set,shift,ret) \
	tryAMAGICunW_var(CAT2(meth,_amg),set,shift,ret)

#define FORCE_SETs(sv) STMT_START { sv_setsv(TARG, (sv)); SETTARG; } STMT_END

#define tryAMAGICun_var(meth_enum) tryAMAGICunW_var(meth_enum,SETsvUN,0,RETURN)
#define tryAMAGICun(meth)	tryAMAGICun_var(CAT2(meth,_amg))
#define tryAMAGICunSET(meth)	tryAMAGICunW(meth,SETs,0,RETURN)
#define tryAMAGICunTARGET(meth, shift)					\
	STMT_START { dSP; sp--; 	/* get TARGET from below PL_stack_sp */		\
	    { dTARGETSTACKED; 						\
		{ dSP; tryAMAGICunW(meth,FORCE_SETs,shift,RETURN);}}} STMT_END

#define setAGAIN(ref)	\
    STMT_START {					\
	sv = ref;					\
	if (!SvROK(ref))				\
	    Perl_croak(aTHX_ "Overloaded dereference did not return a reference");	\
	if (ref != arg && SvRV(ref) != SvRV(arg)) {	\
	    arg = ref;					\
	    goto am_again;				\
	}						\
    } STMT_END

#define tryAMAGICunDEREF(meth) tryAMAGICunW(meth,setAGAIN,0,(void)0)
#define tryAMAGICunDEREF_var(meth_enum) \
	tryAMAGICunW_var(meth_enum,setAGAIN,0,(void)0)

#define opASSIGN (PL_op->op_flags & OPf_STACKED)
#define SETsv(sv)	STMT_START {					\
		if (opASSIGN || (SvFLAGS(TARG) & SVs_PADMY))		\
		   { sv_setsv(TARG, (sv)); SETTARG; }			\
		else SETs(sv); } STMT_END

#define SETsvUN(sv)	STMT_START {					\
		if (SvFLAGS(TARG) & SVs_PADMY)		\
		   { sv_setsv(TARG, (sv)); SETTARG; }			\
		else SETs(sv); } STMT_END

/* newSVsv does not behave as advertised, so we copy missing
 * information by hand */

/* SV* ref causes confusion with the member variable
   changed SV* ref to SV* tmpRef */
#define RvDEEPCP(rv) STMT_START { SV* tmpRef=SvRV(rv);      \
  if (SvREFCNT(tmpRef)>1) {                 \
    SvRV_set(rv, AMG_CALLun(rv,copy));	\
    SvREFCNT_dec(tmpRef);                   \
  } } STMT_END

/*
=for apidoc mU||LVRET
True if this op will be the return value of an lvalue subroutine

=cut */
#define LVRET ((PL_op->op_private & OPpMAYBE_LVSUB) && is_lvalue_sub())

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
