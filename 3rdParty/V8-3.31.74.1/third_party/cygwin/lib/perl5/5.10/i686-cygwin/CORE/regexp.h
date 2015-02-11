/*    regexp.h
 *
 *    Copyright (C) 1993, 1994, 1996, 1997, 1999, 2000, 2001, 2003,
 *    2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#ifndef PLUGGABLE_RE_EXTENSION
/* we don't want to include this stuff if we are inside of
   an external regex engine based on the core one - like re 'debug'*/

struct regnode {
    U8	flags;
    U8  type;
    U16 next_off;
};

typedef struct regnode regnode;

struct reg_substr_data;

struct reg_data;

struct regexp_engine;
struct regexp;

struct reg_substr_datum {
    I32 min_offset;
    I32 max_offset;
    SV *substr;		/* non-utf8 variant */
    SV *utf8_substr;	/* utf8 variant */
    I32 end_shift;
};
struct reg_substr_data {
    struct reg_substr_datum data[3];	/* Actual array */
};

#ifdef PERL_OLD_COPY_ON_WRITE
#define SV_SAVED_COPY   SV *saved_copy; /* If non-NULL, SV which is COW from original */
#else
#define SV_SAVED_COPY
#endif

typedef struct regexp_paren_pair {
    I32 start;
    I32 end;
} regexp_paren_pair;

/*
  The regexp/REGEXP struct, see L<perlreapi> for further documentation
  on the individual fields. The struct is ordered so that the most
  commonly used fields are placed at the start.

  Any patch that adds items to this struct will need to include
  changes to F<sv.c> (C<Perl_re_dup()>) and F<regcomp.c>
  (C<pregfree()>). This involves freeing or cloning items in the
  regexp's data array based on the data item's type.
*/

typedef struct regexp {
        /* what engine created this regexp? */
	const struct regexp_engine* engine; 
	struct regexp* mother_re; /* what re is this a lightweight copy of? */
	
	/* Information about the match that the perl core uses to manage things */
	U32 extflags;           /* Flags used both externally and internally */
	I32 minlen;		/* mininum possible length of string to match */
	I32 minlenret;		/* mininum possible length of $& */
	U32 gofs;               /* chars left of pos that we search from */
	struct reg_substr_data *substrs; /* substring data about strings that must appear
                                   in the final match, used for optimisations */
	U32 nparens;		/* number of capture buffers */

        /* private engine specific data */
	U32 intflags;		/* Engine Specific Internal flags */
	void *pprivate;         /* Data private to the regex engine which 
                                   created this object. */
        
        /* Data about the last/current match. These are modified during matching*/
        U32 lastparen;		/* last open paren matched */
	U32 lastcloseparen;	/* last close paren matched */
        regexp_paren_pair *swap;  /* Swap copy of *offs */ 
        regexp_paren_pair *offs;  /* Array of offsets for (@-) and (@+) */

	char *subbeg;		/* saved or original string 
				   so \digit works forever. */
	SV_SAVED_COPY           /* If non-NULL, SV which is COW from original */
	I32 sublen;		/* Length of string pointed by subbeg */
        
        
        /* Information about the match that isn't often used */
	I32 prelen;		/* length of precomp */
	const char *precomp;	/* pre-compilation regular expression */
	/* wrapped can't be const char*, as it is returned by sv_2pv_flags */
	char *wrapped;          /* wrapped version of the pattern */
	I32 wraplen;		/* length of wrapped */
	I32 seen_evals;         /* number of eval groups in the pattern - for security checks */ 
        HV *paren_names;	/* Optional hash of paren names */
        
        /* Refcount of this regexp */
	I32 refcnt;             /* Refcount of this regexp */
} regexp;

#define RXp_PAREN_NAMES(rx)	((rx)->paren_names)

/* used for high speed searches */
typedef struct re_scream_pos_data_s
{
    char **scream_olds;		/* match pos */
    I32 *scream_pos;		/* Internal iterator of scream. */
} re_scream_pos_data;

/* regexp_engine structure. This is the dispatch table for regexes.
 * Any regex engine implementation must be able to build one of these.
 */
typedef struct regexp_engine {
    REGEXP* (*comp) (pTHX_ const SV * const pattern, const U32 flags);
    I32     (*exec) (pTHX_ REGEXP * const rx, char* stringarg, char* strend,
                     char* strbeg, I32 minend, SV* screamer,
                     void* data, U32 flags);
    char*   (*intuit) (pTHX_ REGEXP * const rx, SV *sv, char *strpos,
                       char *strend, const U32 flags,
                       re_scream_pos_data *data);
    SV*     (*checkstr) (pTHX_ REGEXP * const rx);
    void    (*free) (pTHX_ REGEXP * const rx);
    void    (*numbered_buff_FETCH) (pTHX_ REGEXP * const rx, const I32 paren,
                                    SV * const sv);
    void    (*numbered_buff_STORE) (pTHX_ REGEXP * const rx, const I32 paren,
                                   SV const * const value);
    I32     (*numbered_buff_LENGTH) (pTHX_ REGEXP * const rx, const SV * const sv,
                                    const I32 paren);
    SV*     (*named_buff) (pTHX_ REGEXP * const rx, SV * const key,
                           SV * const value, const U32 flags);
    SV*     (*named_buff_iter) (pTHX_ REGEXP * const rx, const SV * const lastkey,
                                const U32 flags);
    SV*     (*qr_package)(pTHX_ REGEXP * const rx);
#ifdef USE_ITHREADS
    void*   (*dupe) (pTHX_ REGEXP * const rx, CLONE_PARAMS *param);
#endif
} regexp_engine;

/*
  These are passed to the numbered capture variable callbacks as the
  paren name. >= 1 is reserved for actual numbered captures, i.e. $1,
  $2 etc.
*/
#define RX_BUFF_IDX_PREMATCH  -2 /* $` / ${^PREMATCH}  */
#define RX_BUFF_IDX_POSTMATCH -1 /* $' / ${^POSTMATCH} */
#define RX_BUFF_IDX_FULLMATCH      0 /* $& / ${^MATCH}     */

/*
  Flags that are passed to the named_buff and named_buff_iter
  callbacks above. Those routines are called from universal.c via the
  Tie::Hash::NamedCapture interface for %+ and %- and the re::
  functions in the same file.
*/

/* The Tie::Hash::NamedCapture operation this is part of, if any */
#define RXapif_FETCH     0x0001
#define RXapif_STORE     0x0002
#define RXapif_DELETE    0x0004
#define RXapif_CLEAR     0x0008
#define RXapif_EXISTS    0x0010
#define RXapif_SCALAR    0x0020
#define RXapif_FIRSTKEY  0x0040
#define RXapif_NEXTKEY   0x0080

/* Whether %+ or %- is being operated on */
#define RXapif_ONE       0x0100 /* %+ */
#define RXapif_ALL       0x0200 /* %- */

/* Whether this is being called from a re:: function */
#define RXapif_REGNAME         0x0400
#define RXapif_REGNAMES        0x0800
#define RXapif_REGNAMES_COUNT  0x1000 

/*
=head1 REGEXP Functions

=for apidoc Am|REGEXP *|SvRX|SV *sv

Convenience macro to get the REGEXP from a SV. This is approximately
equivalent to the following snippet:

    if (SvMAGICAL(sv))
        mg_get(sv);
    if (SvROK(sv) &&
        (tmpsv = (SV*)SvRV(sv)) &&
        SvTYPE(tmpsv) == SVt_PVMG &&
        (tmpmg = mg_find(tmpsv, PERL_MAGIC_qr)))
    {
        return (REGEXP *)tmpmg->mg_obj;
    }

NULL will be returned if a REGEXP* is not found.

=for apidoc Am|bool|SvRXOK|SV* sv

Returns a boolean indicating whether the SV contains qr magic
(PERL_MAGIC_qr).

If you want to do something with the REGEXP* later use SvRX instead
and check for NULL.

=cut
*/

#define SvRX(sv)   (Perl_get_re_arg(aTHX_ sv))
#define SvRXOK(sv) (Perl_get_re_arg(aTHX_ sv) ? TRUE : FALSE)


/* Flags stored in regexp->extflags 
 * These are used by code external to the regexp engine
 *
 * Note that flags starting with RXf_PMf_ have exact equivalents
 * stored in op_pmflags and which are defined in op.h, they are defined
 * numerically here only for clarity.
 *
 * NOTE: if you modify any RXf flags you should run regen.pl or regcomp.pl
 * so that regnodes.h is updated with the changes. 
 *
 */

/* Anchor and GPOS related stuff */
#define RXf_ANCH_BOL    	0x00000001
#define RXf_ANCH_MBOL   	0x00000002
#define RXf_ANCH_SBOL   	0x00000004
#define RXf_ANCH_GPOS   	0x00000008
#define RXf_GPOS_SEEN   	0x00000010
#define RXf_GPOS_FLOAT  	0x00000020
/* two bits here */
#define RXf_ANCH        	(RXf_ANCH_BOL|RXf_ANCH_MBOL|RXf_ANCH_GPOS|RXf_ANCH_SBOL)
#define RXf_GPOS_CHECK          (RXf_GPOS_SEEN|RXf_ANCH_GPOS)
#define RXf_ANCH_SINGLE         (RXf_ANCH_SBOL|RXf_ANCH_GPOS)

/* Flags indicating special patterns */
#define RXf_SKIPWHITE		0x00000100 /* Pattern is for a split / / */
#define RXf_START_ONLY		0x00000200 /* Pattern is /^/ */
#define RXf_WHITE		0x00000400 /* Pattern is /\s+/ */
#define RXf_NULL		0x40000000 /* Pattern is // */

/* 0x1F800 of extflags is used by (RXf_)PMf_COMPILETIME
 * If you change these you need to change the equivalent flags in op.h, and
 * vice versa.  */
#define RXf_PMf_LOCALE  	0x00000800 /* use locale */
#define RXf_PMf_MULTILINE	0x00001000 /* /m         */
#define RXf_PMf_SINGLELINE	0x00002000 /* /s         */
#define RXf_PMf_FOLD    	0x00004000 /* /i         */
#define RXf_PMf_EXTENDED	0x00008000 /* /x         */
#define RXf_PMf_KEEPCOPY	0x00010000 /* /p         */
/* these flags are transfered from the PMOP->op_pmflags member during compilation */
#define RXf_PMf_STD_PMMOD_SHIFT	12
#define RXf_PMf_STD_PMMOD	(RXf_PMf_MULTILINE|RXf_PMf_SINGLELINE|RXf_PMf_FOLD|RXf_PMf_EXTENDED)
#define RXf_PMf_COMPILETIME	(RXf_PMf_MULTILINE|RXf_PMf_SINGLELINE|RXf_PMf_LOCALE|RXf_PMf_FOLD|RXf_PMf_EXTENDED|RXf_PMf_KEEPCOPY)

#define CASE_STD_PMMOD_FLAGS_PARSE_SET(pmfl)                        \
    case IGNORE_PAT_MOD:    *(pmfl) |= RXf_PMf_FOLD;       break;   \
    case MULTILINE_PAT_MOD: *(pmfl) |= RXf_PMf_MULTILINE;  break;   \
    case SINGLE_PAT_MOD:    *(pmfl) |= RXf_PMf_SINGLELINE; break;   \
    case XTENDED_PAT_MOD:   *(pmfl) |= RXf_PMf_EXTENDED;   break

/* chars and strings used as regex pattern modifiers
 * Singlular is a 'c'har, plural is a "string"
 *
 * NOTE, KEEPCOPY was originally 'k', but was changed to 'p' for preserve
 * for compatibility reasons with Regexp::Common which highjacked (?k:...)
 * for its own uses. So 'k' is out as well.
 */
#define EXEC_PAT_MOD         'e'
#define KEEPCOPY_PAT_MOD     'p'
#define ONCE_PAT_MOD         'o'
#define GLOBAL_PAT_MOD       'g'
#define CONTINUE_PAT_MOD     'c'
#define MULTILINE_PAT_MOD    'm'
#define SINGLE_PAT_MOD       's'
#define IGNORE_PAT_MOD       'i'
#define XTENDED_PAT_MOD      'x'

#define ONCE_PAT_MODS        "o"
#define KEEPCOPY_PAT_MODS    "p"
#define EXEC_PAT_MODS        "e"
#define LOOP_PAT_MODS        "gc"

#define STD_PAT_MODS        "msix"

#define INT_PAT_MODS    STD_PAT_MODS    KEEPCOPY_PAT_MODS

#define EXT_PAT_MODS    ONCE_PAT_MODS   KEEPCOPY_PAT_MODS
#define QR_PAT_MODS     STD_PAT_MODS    EXT_PAT_MODS
#define M_PAT_MODS      QR_PAT_MODS     LOOP_PAT_MODS
#define S_PAT_MODS      M_PAT_MODS      EXEC_PAT_MODS

/*
 * NOTE: if you modify any RXf flags you should run regen.pl or regcomp.pl
 * so that regnodes.h is updated with the changes. 
 *
 */

/* What we have seen */
#define RXf_LOOKBEHIND_SEEN	0x00020000
#define RXf_EVAL_SEEN   	0x00040000
#define RXf_CANY_SEEN   	0x00080000

/* Special */
#define RXf_NOSCAN      	0x00100000
#define RXf_CHECK_ALL   	0x00200000

/* UTF8 related */
#define RXf_UTF8        	0x00400000
#define RXf_MATCH_UTF8  	0x00800000

/* Intuit related */
#define RXf_USE_INTUIT_NOML	0x01000000
#define RXf_USE_INTUIT_ML	0x02000000
#define RXf_INTUIT_TAIL 	0x04000000

/*
  Set in Perl_pmruntime if op_flags & OPf_SPECIAL, i.e. split. Will
  be used by regex engines to check whether they should set
  RXf_SKIPWHITE
*/
#define RXf_SPLIT           0x08000000

#define RXf_USE_INTUIT		(RXf_USE_INTUIT_NOML|RXf_USE_INTUIT_ML)

/* Copy and tainted info */
#define RXf_COPY_DONE   	0x10000000
#define RXf_TAINTED_SEEN	0x20000000
#define RXf_TAINTED             0x80000000 /* this pattern is tainted */

/*
 * NOTE: if you modify any RXf flags you should run regen.pl or regcomp.pl
 * so that regnodes.h is updated with the changes. 
 *
 */

#define RX_HAS_CUTGROUP(prog) ((prog)->intflags & PREGf_CUTGROUP_SEEN)
#define RXp_MATCH_TAINTED(prog)	(RXp_EXTFLAGS(prog) & RXf_TAINTED_SEEN)
#define RX_MATCH_TAINTED(prog)	(RX_EXTFLAGS(prog) & RXf_TAINTED_SEEN)
#define RX_MATCH_TAINTED_on(prog) (RX_EXTFLAGS(prog) |= RXf_TAINTED_SEEN)
#define RX_MATCH_TAINTED_off(prog) (RX_EXTFLAGS(prog) &= ~RXf_TAINTED_SEEN)
#define RX_MATCH_TAINTED_set(prog, t) ((t) \
				       ? RX_MATCH_TAINTED_on(prog) \
				       : RX_MATCH_TAINTED_off(prog))

#define RXp_MATCH_COPIED(prog)		(RXp_EXTFLAGS(prog) & RXf_COPY_DONE)
#define RX_MATCH_COPIED(prog)		(RX_EXTFLAGS(prog) & RXf_COPY_DONE)
#define RXp_MATCH_COPIED_on(prog)	(RXp_EXTFLAGS(prog) |= RXf_COPY_DONE)
#define RX_MATCH_COPIED_on(prog)	(RX_EXTFLAGS(prog) |= RXf_COPY_DONE)
#define RXp_MATCH_COPIED_off(prog)	(RXp_EXTFLAGS(prog) &= ~RXf_COPY_DONE)
#define RX_MATCH_COPIED_off(prog)	(RX_EXTFLAGS(prog) &= ~RXf_COPY_DONE)
#define RX_MATCH_COPIED_set(prog,t)	((t) \
					 ? RX_MATCH_COPIED_on(prog) \
					 : RX_MATCH_COPIED_off(prog))

#define RXp_EXTFLAGS(rx)	((rx)->extflags)

#define RX_PRECOMP(prog)	((prog)->precomp)
#define RX_PRELEN(prog)		((prog)->prelen)
#define RX_WRAPPED(prog)	((prog)->wrapped)
#define RX_WRAPLEN(prog)	((prog)->wraplen)
#define RX_CHECK_SUBSTR(prog)	((prog)->check_substr)
#define RX_EXTFLAGS(prog)	((prog)->extflags)
#define RX_REFCNT(prog)		((prog)->refcnt)
#define RX_ENGINE(prog)		((prog)->engine)
#define RX_SUBBEG(prog)		((prog)->subbeg)
#define RX_OFFS(prog)		((prog)->offs)
#define RX_NPARENS(prog)	((prog)->nparens)
#define RX_SUBLEN(prog)		((prog)->sublen)
#define RX_SUBBEG(prog)		((prog)->subbeg)
#define RX_MINLEN(prog)		((prog)->minlen)
#define RX_MINLENRET(prog)	((prog)->minlenret)
#define RX_GOFS(prog)		((prog)->gofs)
#define RX_LASTPAREN(prog)	((prog)->lastparen)
#define RX_LASTCLOSEPAREN(prog)	((prog)->lastcloseparen)
#define RX_SEEN_EVALS(prog)	((prog)->seen_evals)

#endif /* PLUGGABLE_RE_EXTENSION */

/* Stuff that needs to be included in the plugable extension goes below here */

#ifdef PERL_OLD_COPY_ON_WRITE
#define RX_MATCH_COPY_FREE(rx) \
	STMT_START {if (rx->saved_copy) { \
	    SV_CHECK_THINKFIRST_COW_DROP(rx->saved_copy); \
	} \
	if (RX_MATCH_COPIED(rx)) { \
	    Safefree(RX_SUBBEG(rx)); \
	    RX_MATCH_COPIED_off(rx); \
	}} STMT_END
#else
#define RX_MATCH_COPY_FREE(rx) \
	STMT_START {if (RX_MATCH_COPIED(rx)) { \
	    Safefree(RX_SUBBEG(rx)); \
	    RX_MATCH_COPIED_off(rx); \
	}} STMT_END
#endif

#define RXp_MATCH_UTF8(prog)		(RXp_EXTFLAGS(prog) & RXf_MATCH_UTF8)
#define RX_MATCH_UTF8(prog)		(RX_EXTFLAGS(prog) & RXf_MATCH_UTF8)
#define RX_MATCH_UTF8_on(prog)		(RX_EXTFLAGS(prog) |= RXf_MATCH_UTF8)
#define RX_MATCH_UTF8_off(prog)		(RX_EXTFLAGS(prog) &= ~RXf_MATCH_UTF8)
#define RX_MATCH_UTF8_set(prog, t)	((t) \
			? (RX_MATCH_UTF8_on(prog), (PL_reg_match_utf8 = 1)) \
			: (RX_MATCH_UTF8_off(prog), (PL_reg_match_utf8 = 0)))

/* Whether the pattern stored at RX_WRAPPED is in UTF-8  */
#define RX_UTF8(prog)			(RX_EXTFLAGS(prog) & RXf_UTF8)
    
#define REXEC_COPY_STR	0x01		/* Need to copy the string. */
#define REXEC_CHECKED	0x02		/* check_substr already checked. */
#define REXEC_SCREAM	0x04		/* use scream table. */
#define REXEC_IGNOREPOS	0x08		/* \G matches at start. */
#define REXEC_NOT_FIRST	0x10		/* This is another iteration of //g. */

#define ReREFCNT_inc(re) ((void)(re && re->refcnt++), re)
#define ReREFCNT_dec(re) CALLREGFREE(re)

#define FBMcf_TAIL_DOLLAR	1
#define FBMcf_TAIL_DOLLARM	2
#define FBMcf_TAIL_Z		4
#define FBMcf_TAIL_z		8
#define FBMcf_TAIL		(FBMcf_TAIL_DOLLAR|FBMcf_TAIL_DOLLARM|FBMcf_TAIL_Z|FBMcf_TAIL_z)

#define FBMrf_MULTILINE	1

/* an accepting state/position*/
struct _reg_trie_accepted {
    U8   *endpos;
    U16  wordnum;
};
typedef struct _reg_trie_accepted reg_trie_accepted;

/* some basic information about the current match that is created by
 * Perl_regexec_flags and then passed to regtry(), regmatch() etc */

typedef struct {
    regexp *prog;
    char *bol;
    char *till;
    SV *sv;
    char *ganch;
    char *cutpoint;
} regmatch_info;
 

/* structures for holding and saving the state maintained by regmatch() */

#ifndef MAX_RECURSE_EVAL_NOCHANGE_DEPTH
#define MAX_RECURSE_EVAL_NOCHANGE_DEPTH 1000
#endif

typedef I32 CHECKPOINT;

typedef struct regmatch_state {
    int resume_state;		/* where to jump to on return */
    char *locinput;		/* where to backtrack in string on failure */

    union {

	/* this is a fake union member that matches the first element
	 * of each member that needs to store positive backtrack
	 * information */
	struct {
	    struct regmatch_state *prev_yes_state;
	} yes;

        /* branchlike members */
        /* this is a fake union member that matches the first elements
         * of each member that needs to behave like a branch */
        struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    U32 lastparen;
	    CHECKPOINT cp;
	    
        } branchlike;
        	    
	struct {
	    /* the first elements must match u.branchlike */
	    struct regmatch_state *prev_yes_state;
	    U32 lastparen;
	    CHECKPOINT cp;
	    
	    regnode *next_branch; /* next branch node */
	} branch;

	struct {
	    /* the first elements must match u.branchlike */
	    struct regmatch_state *prev_yes_state;
	    U32 lastparen;
	    CHECKPOINT cp;

	    reg_trie_accepted *accept_buff; /* accepting states we have seen */
	    U32		accepted; /* how many accepting states we have seen */
	    U16         *jump;  /* positive offsets from me */
	    regnode	*B;	/* node following the trie */
	    regnode	*me;	/* Which node am I - needed for jump tries*/
	} trie;

        /* special types - these members are used to store state for special
           regops like eval, if/then, lookaround and the markpoint state */
	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *prev_eval;
	    struct regmatch_state *prev_curlyx;
	    regexp	*prev_rex;
	    U32		toggle_reg_flags; /* what bits in PL_reg_flags to
					    flip when transitioning between
					    inner and outer rexen */
	    CHECKPOINT	cp;	/* remember current savestack indexes */
	    CHECKPOINT	lastcp;
	    U32        close_paren; /* which close bracket is our end */
	    regnode	*B;	/* the node following us  */
	} eval;

	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    I32 wanted;
	    I32 logical;	/* saved copy of 'logical' var */
	    regnode  *me; /* the IFMATCH/SUSPEND/UNLESSM node  */
	} ifmatch; /* and SUSPEND/UNLESSM */
	
	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *prev_mark;
	    SV* mark_name;
	    char *mark_loc;
	} mark;
	
	struct {
	    int val;
	} keeper;

        /* quantifiers - these members are used for storing state for
           for the regops used to implement quantifiers */
	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *prev_curlyx; /* previous cur_curlyx */
	    regnode	*A, *B;	/* the nodes corresponding to /A*B/  */
	    CHECKPOINT	cp;	/* remember current savestack index */
	    bool	minmod;
	    int		parenfloor;/* how far back to strip paren data */
	    int		min;	/* the minimal number of A's to match */
	    int		max;	/* the maximal number of A's to match */

	    /* these two are modified by WHILEM */
	    int		count;	/* how many instances of A we've matched */
	    char	*lastloc;/* where previous A matched (0-len detect) */
	} curlyx;

	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *save_curlyx;
	    CHECKPOINT	cp;	/* remember current savestack indexes */
	    CHECKPOINT	lastcp;
	    char	*save_lastloc;	/* previous curlyx.lastloc */
	    I32		cache_offset;
	    I32		cache_mask;
	} whilem;

	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    I32 c1, c2;		/* case fold search */
	    CHECKPOINT cp;
	    I32 alen;		/* length of first-matched A string */
	    I32 count;
	    bool minmod;
	    regnode *A, *B;	/* the nodes corresponding to /A*B/  */
	    regnode *me;	/* the curlym node */
	} curlym;

	struct {
	    U32 paren;
	    CHECKPOINT cp;
	    I32 c1, c2;		/* case fold search */
	    char *maxpos;	/* highest possible point in string to match */
	    char *oldloc;	/* the previous locinput */
	    int count;
	    int min, max;	/* {m,n} */
	    regnode *A, *B;	/* the nodes corresponding to /A*B/  */
	} curly; /* and CURLYN/PLUS/STAR */

    } u;
} regmatch_state;

/* how many regmatch_state structs to allocate as a single slab.
 * We do it in 4K blocks for efficiency. The "3" is 2 for the next/prev
 * pointers, plus 1 for any mythical malloc overhead. */
 
#define PERL_REGMATCH_SLAB_SLOTS \
    ((4096 - 3 * sizeof (void*)) / sizeof(regmatch_state))

typedef struct regmatch_slab {
    regmatch_state states[PERL_REGMATCH_SLAB_SLOTS];
    struct regmatch_slab *prev, *next;
} regmatch_slab;

#define PL_reg_flags		PL_reg_state.re_state_reg_flags
#define PL_bostr		PL_reg_state.re_state_bostr
#define PL_reginput		PL_reg_state.re_state_reginput
#define PL_regeol		PL_reg_state.re_state_regeol
#define PL_regoffs		PL_reg_state.re_state_regoffs
#define PL_reglastparen		PL_reg_state.re_state_reglastparen
#define PL_reglastcloseparen	PL_reg_state.re_state_reglastcloseparen
#define PL_reg_start_tmp	PL_reg_state.re_state_reg_start_tmp
#define PL_reg_start_tmpl	PL_reg_state.re_state_reg_start_tmpl
#define PL_reg_eval_set		PL_reg_state.re_state_reg_eval_set
#define PL_reg_match_utf8	PL_reg_state.re_state_reg_match_utf8
#define PL_reg_magic		PL_reg_state.re_state_reg_magic
#define PL_reg_oldpos		PL_reg_state.re_state_reg_oldpos
#define PL_reg_oldcurpm		PL_reg_state.re_state_reg_oldcurpm
#define PL_reg_curpm		PL_reg_state.re_state_reg_curpm
#define PL_reg_oldsaved		PL_reg_state.re_state_reg_oldsaved
#define PL_reg_oldsavedlen	PL_reg_state.re_state_reg_oldsavedlen
#define PL_reg_maxiter		PL_reg_state.re_state_reg_maxiter
#define PL_reg_leftiter		PL_reg_state.re_state_reg_leftiter
#define PL_reg_poscache		PL_reg_state.re_state_reg_poscache
#define PL_reg_poscache_size	PL_reg_state.re_state_reg_poscache_size
#define PL_regsize		PL_reg_state.re_state_regsize
#define PL_reg_starttry		PL_reg_state.re_state_reg_starttry
#define PL_nrs			PL_reg_state.re_state_nrs

struct re_save_state {
    U32 re_state_reg_flags;		/* from regexec.c */
    U32 re_state_reg_start_tmpl;	/* from regexec.c */
    I32 re_state_reg_eval_set;		/* from regexec.c */
    bool re_state_reg_match_utf8;	/* from regexec.c */
    char *re_state_bostr;
    char *re_state_reginput;		/* String-input pointer. */
    char *re_state_regeol;		/* End of input, for $ check. */
    regexp_paren_pair *re_state_regoffs;  /* Pointer to start/end pairs */
    U32 *re_state_reglastparen;		/* Similarly for lastparen. */
    U32 *re_state_reglastcloseparen;	/* Similarly for lastcloseparen. */
    char **re_state_reg_start_tmp;	/* from regexec.c */
    MAGIC *re_state_reg_magic;		/* from regexec.c */
    PMOP *re_state_reg_oldcurpm;	/* from regexec.c */
    PMOP *re_state_reg_curpm;		/* from regexec.c */
    char *re_state_reg_oldsaved;	/* old saved substr during match */
    STRLEN re_state_reg_oldsavedlen;	/* old length of saved substr during match */
    STRLEN re_state_reg_poscache_size;	/* size of pos cache of WHILEM */
    I32 re_state_reg_oldpos;		/* from regexec.c */
    I32 re_state_reg_maxiter;		/* max wait until caching pos */
    I32 re_state_reg_leftiter;		/* wait until caching pos */
    U32 re_state_regsize;		/* from regexec.c */
    char *re_state_reg_poscache;	/* cache of pos of WHILEM */
    char *re_state_reg_starttry;	/* from regexec.c */
#ifdef PERL_OLD_COPY_ON_WRITE
    SV *re_state_nrs;			/* was placeholder: unused since 5.8.0 (5.7.2 patch #12027 for bug ID 20010815.012). Used to save rx->saved_copy */
#endif
};

#define SAVESTACK_ALLOC_FOR_RE_SAVE_STATE \
	(1 + ((sizeof(struct re_save_state) - 1) / sizeof(*PL_savestack)))

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
