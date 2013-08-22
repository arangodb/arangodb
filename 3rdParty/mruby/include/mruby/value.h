/*
** mruby/value.h - mrb_value definition
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_VALUE_H
#define MRUBY_VALUE_H

#ifdef MRB_USE_FLOAT
  typedef float mrb_float;
# define mrb_float_to_str(buf, i) sprintf(buf, "%.7e", i)
# define str_to_mrb_float(buf) strtof(buf, NULL)
#else
  typedef double mrb_float;
# define mrb_float_to_str(buf, i) sprintf(buf, "%.16e", i)
# define str_to_mrb_float(buf) strtod(buf, NULL)
#endif

#if defined(MRB_INT16) && defined(MRB_INT64)
# error "You can't define MRB_INT16 and MRB_INT64 at the same time."
#endif

#if defined(MRB_INT64)
# ifdef MRB_NAN_BOXING
#  error Cannot use NaN boxing when mrb_int is 64bit
# else
   typedef int64_t mrb_int;
#  define MRB_INT_MIN INT64_MIN
#  define MRB_INT_MAX INT64_MAX
#  define PRIdMRB_INT PRId64
#  define PRIiMRB_INT PRIi64
#  define PRIoMRB_INT PRIo64
#  define PRIxMRB_INT PRIx64
#  define PRIXMRB_INT PRIX64
# endif
#elif defined(MRB_INT16)
  typedef int16_t mrb_int;
# define MRB_INT_MIN INT16_MIN
# define MRB_INT_MAX INT16_MAX
#else
  typedef int32_t mrb_int;
# define MRB_INT_MIN INT32_MIN
# define MRB_INT_MAX INT32_MAX
# define PRIdMRB_INT PRId32
# define PRIiMRB_INT PRIi32
# define PRIoMRB_INT PRIo32
# define PRIxMRB_INT PRIx32
# define PRIXMRB_INT PRIX32
#endif
typedef short mrb_sym;

#ifdef _MSC_VER
# ifndef __cplusplus
#  define inline __inline
# endif
# define snprintf _snprintf
# if _MSC_VER < 1800
#  include <float.h>
#  define isnan _isnan
#  define isinf(n) (!_finite(n) && !_isnan(n))
#  define strtoll _strtoi64
#  define strtof (float)strtod
#  define PRId32 "I32d"
#  define PRIi32 "I32i"
#  define PRIo32 "I32o"
#  define PRIx32 "I32x"
#  define PRIX32 "I32X"
#  define PRId64 "I64d"
#  define PRIi64 "I64i"
#  define PRIo64 "I64o"
#  define PRIx64 "I64x"
#  define PRIX64 "I64X"
# else
#  include <inttypes.h>
# endif
#else
# include <inttypes.h>
#endif

typedef uint8_t mrb_bool;
struct mrb_state;

#if defined(MRB_NAN_BOXING)

#ifdef MRB_USE_FLOAT
# error ---->> MRB_NAN_BOXING and MRB_USE_FLOAT conflict <<----
#endif

#ifdef MRB_INT64
# error ---->> MRB_NAN_BOXING and MRB_INT64 conflict <<----
#endif

enum mrb_vtype {
  MRB_TT_FALSE = 1,   /*   1 */
  MRB_TT_FREE,        /*   2 */
  MRB_TT_TRUE,        /*   3 */
  MRB_TT_FIXNUM,      /*   4 */
  MRB_TT_SYMBOL,      /*   5 */
  MRB_TT_UNDEF,       /*   6 */
  MRB_TT_FLOAT,       /*   7 */
  MRB_TT_VOIDP,       /*   8 */
  MRB_TT_OBJECT,      /*   9 */
  MRB_TT_CLASS,       /*  10 */
  MRB_TT_MODULE,      /*  11 */
  MRB_TT_ICLASS,      /*  12 */
  MRB_TT_SCLASS,      /*  13 */
  MRB_TT_PROC,        /*  14 */
  MRB_TT_ARRAY,       /*  15 */
  MRB_TT_HASH,        /*  16 */
  MRB_TT_STRING,      /*  17 */
  MRB_TT_RANGE,       /*  18 */
  MRB_TT_EXCEPTION,   /*  19 */
  MRB_TT_FILE,        /*  20 */
  MRB_TT_ENV,         /*  21 */
  MRB_TT_DATA,        /*  22 */
  MRB_TT_FIBER,       /*  23 */
  MRB_TT_MAXDEFINE    /*  24 */
};

#define MRB_TT_HAS_BASIC  MRB_TT_OBJECT

#ifdef MRB_ENDIAN_BIG
#define MRB_ENDIAN_LOHI(a,b) a b
#else
#define MRB_ENDIAN_LOHI(a,b) b a
#endif

typedef struct mrb_value {
  union {
    mrb_float f;
    union {
      void *p;
      struct {
	MRB_ENDIAN_LOHI(
 	  uint32_t ttt;
          ,union {
	    mrb_int i;
	    mrb_sym sym;
	  };
        )
      };
    } value;
  };
} mrb_value;

/* value representation by nan-boxing:
 *   float : FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF
 *   object: 111111111111TTTT TTPPPPPPPPPPPPPP PPPPPPPPPPPPPPPP PPPPPPPPPPPPPPPP
 *   int   : 1111111111110001 0000000000000000 IIIIIIIIIIIIIIII IIIIIIIIIIIIIIII
 *   sym   : 1111111111110001 0100000000000000 SSSSSSSSSSSSSSSS SSSSSSSSSSSSSSSS
 * In order to get enough bit size to save TT, all pointers are shifted 2 bits
 * in the right direction.
 */
#define mrb_tt(o)       (((o).value.ttt & 0xfc000)>>14)
#define mrb_mktt(tt)    (0xfff00000|((tt)<<14))
#define mrb_type(o)     ((uint32_t)0xfff00000 < (o).value.ttt ? mrb_tt(o) : MRB_TT_FLOAT)
#define mrb_ptr(o)      ((void*)((((intptr_t)0x3fffffffffff)&((intptr_t)((o).value.p)))<<2))
#define mrb_float(o)    (o).f

#define MRB_SET_VALUE(o, tt, attr, v) do {\
  (o).value.ttt = mrb_mktt(tt);\
  switch (tt) {\
  case MRB_TT_FALSE:\
  case MRB_TT_TRUE:\
  case MRB_TT_UNDEF:\
  case MRB_TT_FIXNUM:\
  case MRB_TT_SYMBOL: (o).attr = (v); break;\
  default: (o).value.i = 0; (o).value.p = (void*)((intptr_t)(o).value.p | (((intptr_t)(v))>>2)); break;\
  }\
} while (0)

static inline mrb_value
mrb_float_value(struct mrb_state *mrb, mrb_float f)
{
  mrb_value v;

  if (f != f) {
    v.value.ttt = 0x7ff80000;
    v.value.i = 0;
  } else {
    v.f = f;
  }
  return v;
}

#else

enum mrb_vtype {
  MRB_TT_FALSE = 0,   /*   0 */
  MRB_TT_FREE,        /*   1 */
  MRB_TT_TRUE,        /*   2 */
  MRB_TT_FIXNUM,      /*   3 */
  MRB_TT_SYMBOL,      /*   4 */
  MRB_TT_UNDEF,       /*   5 */
  MRB_TT_FLOAT,       /*   6 */
  MRB_TT_VOIDP,       /*   7 */
  MRB_TT_OBJECT,      /*   8 */
  MRB_TT_CLASS,       /*   9 */
  MRB_TT_MODULE,      /*  10 */
  MRB_TT_ICLASS,      /*  11 */
  MRB_TT_SCLASS,      /*  12 */
  MRB_TT_PROC,        /*  13 */
  MRB_TT_ARRAY,       /*  14 */
  MRB_TT_HASH,        /*  15 */
  MRB_TT_STRING,      /*  16 */
  MRB_TT_RANGE,       /*  17 */
  MRB_TT_EXCEPTION,   /*  18 */
  MRB_TT_FILE,        /*  19 */
  MRB_TT_ENV,         /*  20 */
  MRB_TT_DATA,        /*  21 */
  MRB_TT_FIBER,       /*  22 */
  MRB_TT_MAXDEFINE    /*  23 */
};

#if defined(MRB_WORD_BOXING)

#define MRB_TT_HAS_BASIC  MRB_TT_FLOAT

enum mrb_special_consts {
  MRB_Qnil    = 0,
  MRB_Qfalse  = 2,
  MRB_Qtrue   = 4,
  MRB_Qundef  = 6,
};

#define MRB_FIXNUM_FLAG   0x01
#define MRB_FIXNUM_SHIFT  1
#define MRB_SYMBOL_FLAG   0x0e
#define MRB_SPECIAL_SHIFT 8

typedef union mrb_value {
  union {
    void *p;
    struct {
      unsigned int i_flag : MRB_FIXNUM_SHIFT;
      mrb_int i : (sizeof(mrb_int) * CHAR_BIT - MRB_FIXNUM_SHIFT);
    };
    struct {
      unsigned int sym_flag : MRB_SPECIAL_SHIFT;
      int sym : (sizeof(mrb_sym) * CHAR_BIT);
    };
    struct RBasic *bp;
    struct RFloat *fp;
    struct RVoidp *vp;
  } value;
  unsigned long w;
} mrb_value;

#define mrb_ptr(o)      (o).value.p
#define mrb_float(o)    (o).value.fp->f

#define MRB_SET_VALUE(o, ttt, attr, v) do {\
  (o).w = 0;\
  (o).attr = (v);\
  switch (ttt) {\
  case MRB_TT_FALSE:  (o).w = (v) ? MRB_Qfalse : MRB_Qnil; break;\
  case MRB_TT_TRUE:   (o).w = MRB_Qtrue; break;\
  case MRB_TT_UNDEF:  (o).w = MRB_Qundef; break;\
  case MRB_TT_FIXNUM: (o).value.i_flag = MRB_FIXNUM_FLAG; break;\
  case MRB_TT_SYMBOL: (o).value.sym_flag = MRB_SYMBOL_FLAG; break;\
  default:            if ((o).value.bp) (o).value.bp->tt = ttt; break;\
  }\
} while (0)

extern mrb_value
mrb_float_value(struct mrb_state *mrb, mrb_float f);

#else /* No MRB_xxx_BOXING */

#define MRB_TT_HAS_BASIC  MRB_TT_OBJECT

typedef struct mrb_value {
  union {
    mrb_float f;
    void *p;
    mrb_int i;
    mrb_sym sym;
  } value;
  enum mrb_vtype tt;
} mrb_value;

#define mrb_type(o)     (o).tt
#define mrb_ptr(o)      (o).value.p
#define mrb_float(o)    (o).value.f

#define MRB_SET_VALUE(o, ttt, attr, v) do {\
  (o).tt = ttt;\
  (o).attr = v;\
} while (0)

static inline mrb_value
mrb_float_value(struct mrb_state *mrb, mrb_float f)
{
  mrb_value v;
  (void) mrb;

  MRB_SET_VALUE(v, MRB_TT_FLOAT, value.f, f);
  return v;
}

#endif  /* no boxing */

#endif

#ifdef MRB_WORD_BOXING

#define mrb_voidp(o) (o).value.vp->p
#define mrb_fixnum_p(o) ((o).value.i_flag == MRB_FIXNUM_FLAG)
#define mrb_undef_p(o) ((o).w == MRB_Qundef)
#define mrb_nil_p(o)  ((o).w == MRB_Qnil)
#define mrb_bool(o)   ((o).w != MRB_Qnil && (o).w != MRB_Qfalse)

#else
#define mrb_voidp(o) mrb_ptr(o)
#define mrb_fixnum_p(o) (mrb_type(o) == MRB_TT_FIXNUM)
#define mrb_undef_p(o) (mrb_type(o) == MRB_TT_UNDEF)
#define mrb_nil_p(o)  (mrb_type(o) == MRB_TT_FALSE && !(o).value.i)
#define mrb_bool(o)   (mrb_type(o) != MRB_TT_FALSE)

#endif  /* no boxing */

#define mrb_fixnum(o) (o).value.i
#define mrb_symbol(o) (o).value.sym
#define mrb_float_p(o) (mrb_type(o) == MRB_TT_FLOAT)
#define mrb_symbol_p(o) (mrb_type(o) == MRB_TT_SYMBOL)
#define mrb_array_p(o) (mrb_type(o) == MRB_TT_ARRAY)
#define mrb_string_p(o) (mrb_type(o) == MRB_TT_STRING)
#define mrb_hash_p(o) (mrb_type(o) == MRB_TT_HASH)
#define mrb_voidp_p(o) (mrb_type(o) == MRB_TT_VOIDP)
#define mrb_test(o)   mrb_bool(o)

#define MRB_OBJECT_HEADER \
  enum mrb_vtype tt:8;\
  uint32_t color:3;\
  uint32_t flags:21;\
  struct RClass *c;\
  struct RBasic *gcnext

/* white: 011, black: 100, gray: 000 */
#define MRB_GC_GRAY 0
#define MRB_GC_WHITE_A 1
#define MRB_GC_WHITE_B (1 << 1)
#define MRB_GC_BLACK (1 << 2)
#define MRB_GC_WHITES (MRB_GC_WHITE_A | MRB_GC_WHITE_B)
#define MRB_GC_COLOR_MASK 7

#define paint_gray(o) ((o)->color = MRB_GC_GRAY)
#define paint_black(o) ((o)->color = MRB_GC_BLACK)
#define paint_white(o) ((o)->color = MRB_GC_WHITES)
#define paint_partial_white(s, o) ((o)->color = (s)->current_white_part)
#define is_gray(o) ((o)->color == MRB_GC_GRAY)
#define is_white(o) ((o)->color & MRB_GC_WHITES)
#define is_black(o) ((o)->color & MRB_GC_BLACK)
#define is_dead(s, o) (((o)->color & other_white_part(s) & MRB_GC_WHITES) || (o)->tt == MRB_TT_FREE)
#define flip_white_part(s) ((s)->current_white_part = other_white_part(s))
#define other_white_part(s) ((s)->current_white_part ^ MRB_GC_WHITES)

struct RBasic {
  MRB_OBJECT_HEADER;
};
#define mrb_basic_ptr(v) ((struct RBasic*)(mrb_ptr(v)))
/* obsolete macro mrb_basic; will be removed soon */
#define mrb_basic(v)     mrb_basic_ptr(v)

struct RObject {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
};
#define mrb_obj_ptr(v)   ((struct RObject*)(mrb_ptr(v)))
/* obsolete macro mrb_object; will be removed soon */
#define mrb_object(o) mrb_obj_ptr(o)
#define mrb_immediate_p(x) (mrb_type(x) <= MRB_TT_VOIDP)
#define mrb_special_const_p(x) mrb_immediate_p(x)

struct RFiber {
  MRB_OBJECT_HEADER;
  struct mrb_context *cxt;
};

#ifdef MRB_WORD_BOXING
struct RFloat {
  MRB_OBJECT_HEADER;
  mrb_float f;
};

struct RVoidp {
  MRB_OBJECT_HEADER;
  void *p;
};

static inline enum mrb_vtype
mrb_type(mrb_value o)
{
  switch (o.w) {
  case MRB_Qfalse:
  case MRB_Qnil:
    return MRB_TT_FALSE;
  case MRB_Qtrue:
    return MRB_TT_TRUE;
  case MRB_Qundef:
    return MRB_TT_UNDEF;
  }
  if (o.value.i_flag == MRB_FIXNUM_FLAG) {
    return MRB_TT_FIXNUM;
  }
  if (o.value.sym_flag == MRB_SYMBOL_FLAG) {
    return MRB_TT_SYMBOL;
  }
  return o.value.bp->tt;
}
#endif  /* MRB_WORD_BOXING */

static inline mrb_value
mrb_fixnum_value(mrb_int i)
{
  mrb_value v;

  MRB_SET_VALUE(v, MRB_TT_FIXNUM, value.i, i);
  return v;
}

static inline mrb_value
mrb_symbol_value(mrb_sym i)
{
  mrb_value v;

  MRB_SET_VALUE(v, MRB_TT_SYMBOL, value.sym, i);
  return v;
}

static inline mrb_value
mrb_obj_value(void *p)
{
  mrb_value v;
  struct RBasic *b = (struct RBasic*)p;

  MRB_SET_VALUE(v, b->tt, value.p, p);
  return v;
}

#ifdef MRB_WORD_BOXING
mrb_value
mrb_voidp_value(struct mrb_state *mrb, void *p);
#else
static inline mrb_value
mrb_voidp_value(struct mrb_state *mrb, void *p)
{
  mrb_value v;
  (void) mrb;

  MRB_SET_VALUE(v, MRB_TT_VOIDP, value.p, p);
  return v;
}
#endif

static inline mrb_value
mrb_false_value(void)
{
  mrb_value v;

  MRB_SET_VALUE(v, MRB_TT_FALSE, value.i, 1);
  return v;
}

static inline mrb_value
mrb_nil_value(void)
{
  mrb_value v;

  MRB_SET_VALUE(v, MRB_TT_FALSE, value.i, 0);
  return v;
}

static inline mrb_value
mrb_true_value(void)
{
  mrb_value v;

  MRB_SET_VALUE(v, MRB_TT_TRUE, value.i, 1);
  return v;
}

static inline mrb_value
mrb_undef_value(void)
{
  mrb_value v;

  MRB_SET_VALUE(v, MRB_TT_UNDEF, value.i, 0);
  return v;
}

static inline mrb_value
mrb_bool_value(mrb_bool boolean)
{
  mrb_value v;

  MRB_SET_VALUE(v, boolean ? MRB_TT_TRUE : MRB_TT_FALSE, value.i, 1);
  return v;
}

#endif  /* MRUBY_OBJECT_H */
