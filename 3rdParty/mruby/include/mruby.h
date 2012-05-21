/*
** mruby - An embeddable Ruby implementaion
**
** Copyright (c) mruby developers 2010-2012
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#ifndef MRUBY_H
#define MRUBY_H

#include <stdlib.h>
#include "mrbconf.h"

enum mrb_vtype {
  MRB_TT_FALSE = 0,   /*   0 */
  MRB_TT_FREE,        /*   1 */
  MRB_TT_TRUE,        /*   2 */
  MRB_TT_FIXNUM,      /*   3 */
  MRB_TT_SYMBOL,      /*   4 */
  MRB_TT_UNDEF,       /*   5 */
  MRB_TT_FLOAT,       /*   6 */
  MRB_TT_OBJECT,      /*   7 */
  MRB_TT_CLASS,       /*   8 */
  MRB_TT_MODULE,      /*   9 */
  MRB_TT_ICLASS,      /*  10 */
  MRB_TT_SCLASS,      /*  11 */
  MRB_TT_PROC,        /*  12 */
  MRB_TT_ARRAY,       /*  13 */
  MRB_TT_HASH,        /*  14 */
  MRB_TT_STRING,      /*  15 */
  MRB_TT_RANGE,       /*  16 */
  MRB_TT_REGEX,       /*  17 */
  MRB_TT_STRUCT,      /*  18 */
  MRB_TT_EXCEPTION,   /*  19 */
  MRB_TT_MATCH,       /*  20 */
  MRB_TT_FILE,        /*  21 */
  MRB_TT_ENV,         /*  22 */
  MRB_TT_DATA,        /*  23 */
  MRB_TT_THREAD,      /*  24 */
  MRB_TT_THREADGRP,   /*  25 */
  MRB_TT_MAXDEFINE    /*  26 */
};

typedef struct mrb_value {
  union {
    mrb_float f;
    void *p;
    mrb_int i;
    mrb_sym sym;
  } value;
  enum mrb_vtype tt:8;
} mrb_value;

#define mrb_type(o)   (o).tt
#define mrb_nil_p(o)  ((o).tt == MRB_TT_FALSE && !(o).value.i)
#define mrb_test(o)   ((o).tt != MRB_TT_FALSE)
#define mrb_fixnum(o) (o).value.i
#define mrb_float(o)  (o).value.f
#define mrb_symbol(o) (o).value.sym
#define mrb_object(o) (o).value.p
#define FIXNUM_P(o)   ((o).tt == MRB_TT_FIXNUM)
#define UNDEF_P(o)    ((o).tt == MRB_TT_UNDEF)

#include "mruby/object.h"

#define IMMEDIATE_P(x) (mrb_type(x) <= MRB_TT_FLOAT)
#define SPECIAL_CONST_P(x) IMMEDIATE_P(x)
#define SYMBOL_P(o) (mrb_type(o) == MRB_TT_SYMBOL)
#define RTEST(o) mrb_test(o)

#define FL_ABLE(x) (!SPECIAL_CONST_P(x))
#define FL_TEST(x,f) (FL_ABLE(x)?(RBASIC(x)->flags&(f)):0)
#define FL_ANY(x,f) FL_TEST(x,f)
#define FL_ALL(x,f) (FL_TEST(x,f) == (f))
#define FL_SET(x,f) do {if (FL_ABLE(x)) RBASIC(x)->flags |= (f);} while (0)
#define FL_UNSET(x,f) do {if (FL_ABLE(x)) RBASIC(x)->flags &= ~(f);} while (0)

static inline mrb_int
mrb_special_const_p(mrb_value obj)
{
  if (SPECIAL_CONST_P(obj)) return 1;
  return 0;
}
static inline mrb_value
mrb_fixnum_value(mrb_int i)
{
  mrb_value v;

  v.tt = MRB_TT_FIXNUM;
  v.value.i = i;
  return v;
}

static inline mrb_value
mrb_float_value(mrb_float f)
{
  mrb_value v;

  v.tt = MRB_TT_FLOAT;
  v.value.f = f;
  return v;
}

static inline mrb_value
mrb_symbol_value(mrb_sym i)
{
  mrb_value v;

  v.tt = MRB_TT_SYMBOL;
  v.value.sym = i;
  return v;
}

static inline mrb_value
mrb_obj_value(void *p)
{
  mrb_value v;
  struct RBasic *b = (struct RBasic*) p;

  v.tt = b->tt;
  v.value.p = p;
  return v;
}

static inline mrb_value
mrb_false_value()
{
  mrb_value v;

  v.tt = MRB_TT_FALSE;
  v.value.i = 1;
  return v;
}

static inline mrb_value
mrb_nil_value()
{
  mrb_value v;

  v.tt = MRB_TT_FALSE;
  v.value.p = 0;
  return v;
}

static inline mrb_value
mrb_true_value()
{
  mrb_value v;

  v.tt = MRB_TT_TRUE;
  v.value.i = 1;
  return v;
}

static inline mrb_value
mrb_undef_value()
{
  mrb_value v;

  v.tt = MRB_TT_UNDEF;
  v.value.i = 0;
  return v;
}

typedef int32_t mrb_code;

struct mrb_state;

typedef void* (*mrb_allocf) (struct mrb_state *mrb, void*, size_t);

#define MRB_ARENA_SIZE 1024  //256 up kusuda 2011/04/30
#define ruby_debug   (mrb_nil_value())
#define ruby_verbose (mrb_nil_value())

typedef struct {
  mrb_sym mid;
  struct RProc *proc;
  int stackidx;
  int nregs;
  int argc;
  mrb_code *pc;
  int acc;
  struct RClass *target_class;
  int ridx;
  int eidx;
  struct REnv *env;
} mrb_callinfo;

enum gc_state {
  GC_STATE_NONE = 0,
  GC_STATE_MARK,
  GC_STATE_SWEEP
};

typedef struct mrb_state {
  void *jmp;

  mrb_allocf allocf;

  mrb_value *stack;
  mrb_value *stbase, *stend;

  mrb_callinfo *ci;
  mrb_callinfo *cibase, *ciend;

  mrb_code **rescue;
  int rsize;
  struct RProc **ensure;
  int esize;

  struct RObject *exc;
  struct kh_iv *globals;

  struct mrb_irep **irep;
  size_t irep_len, irep_capa;

  struct RClass *object_class;
  struct RClass *class_class;
  struct RClass *module_class;
  struct RClass *proc_class;
  struct RClass *string_class;
  struct RClass *array_class;
  struct RClass *hash_class;

  struct RClass *float_class;
  struct RClass *fixnum_class;
  struct RClass *true_class;
  struct RClass *false_class;
  struct RClass *nil_class;
  struct RClass *symbol_class;

  struct RClass *kernel_module;
  struct heap_page *heaps;
  struct heap_page *sweeps;
  struct heap_page *free_heaps;
  size_t live; /* count of live objects */
  struct RBasic *arena[MRB_ARENA_SIZE];
  int arena_idx;

  enum gc_state gc_state; /* state of gc */
  int current_white_part; /* make white object by white_part */
  struct RBasic *gray_list; /* list of gray objects */
  struct RBasic *variable_gray_list; /* list of objects to be traversed atomically */
  size_t gc_live_after_mark;
  size_t gc_threshold;
  mrb_int gc_interval_ratio;
  mrb_int gc_step_ratio;

  mrb_sym symidx;
  struct kh_n2s *name2sym;      /* symbol table */
  struct kh_s2n *sym2name;      /* reverse symbol table */
#ifdef INCLUDE_REGEXP
  struct RNode *local_svar;/* regexp */
#endif

  struct RClass *eException_class;
  struct RClass *eStandardError_class;
  struct RClass *eRuntimeError_class;
} mrb_state;

typedef mrb_value (*mrb_func_t)(mrb_state *mrb, mrb_value);
typedef mrb_value (*mrb_funcargv_t)(mrb_state *mrb, mrb_value, int argc, mrb_value* argv);
struct RClass *mrb_define_class(mrb_state *, const char*, struct RClass*);
struct RClass *mrb_define_module(mrb_state *, const char*);
mrb_value mrb_singleton_class(mrb_state*, mrb_value);
void mrb_include_module(mrb_state*, struct RClass*, struct RClass*);

void mrb_define_method(mrb_state*, struct RClass*, const char*, mrb_func_t,int);
void mrb_define_class_method(mrb_state *, struct RClass *, const char *, mrb_func_t, int);
void mrb_define_singleton_method(mrb_state*, void*, const char*, mrb_func_t,int);
void mrb_define_const(mrb_state*, struct RClass*, const char *name, mrb_value);
mrb_value mrb_instance_new(mrb_state *mrb, mrb_value cv);
struct RClass * mrb_class_new(mrb_state *mrb, struct RClass *super);
struct RClass * mrb_module_new(mrb_state *mrb);
struct RClass * mrb_class_from_sym(mrb_state *mrb, struct RClass *klass, mrb_sym name);
struct RClass * mrb_class_get(mrb_state *mrb, const char *name);
struct RClass * mrb_class_obj_get(mrb_state *mrb, const char *name);

mrb_value mrb_obj_dup(mrb_state *mrb, mrb_value obj);
mrb_value mrb_check_to_integer(mrb_state *mrb, mrb_value val, const char *method);
int mrb_obj_respond_to(struct RClass* c, mrb_sym mid);
struct RClass * mrb_define_class_under(mrb_state *mrb, struct RClass *outer, const char *name, struct RClass *super);
struct RClass * mrb_define_module_under(mrb_state *mrb, struct RClass *outer, const char *name);

/* required arguments */
#define ARGS_REQ(n)     (((n)&0x1f) << 19)
/* optional arguments */
#define ARGS_OPT(n)     (((n)&0x1f) << 14)
/* rest argument */
#define ARGS_REST()     (1 << 13)
/* required arguments after rest */
#define ARGS_POST(n)    (((n)&0x1f) << 8)
/* keyword arguments (n of keys, kdict) */
#define ARGS_KEY(n1,n2) ((((n1)&0x1f) << 3) | ((n2)?(1<<2):0))
/* block argument */
#define ARGS_BLOCK()    (1 << 1)

/* accept any number of arguments */
#define ARGS_ANY()      ARGS_REST()
/* accept no arguments */
#define ARGS_NONE()     0

int mrb_get_args(mrb_state *mrb, const char *format, ...);

mrb_value mrb_funcall(mrb_state*, mrb_value, const char*, int,...);
mrb_value mrb_funcall_argv(mrb_state*, mrb_value, const char*, int, mrb_value*);
mrb_value mrb_funcall_with_block(mrb_state*, mrb_value, const char*, int, mrb_value*, struct RProc*);
mrb_sym mrb_intern(mrb_state*,const char*);
const char *mrb_sym2name(mrb_state*,mrb_sym);
mrb_value mrb_str_format(mrb_state *, int, const mrb_value *, mrb_value);

void *mrb_malloc(mrb_state*, size_t);
void *mrb_calloc(mrb_state*, size_t, size_t);
void *mrb_realloc(mrb_state*, void*, size_t);
void *mrb_obj_alloc(mrb_state*, enum mrb_vtype, struct RClass*);
void *mrb_free(mrb_state*, void*);

mrb_value mrb_str_new(mrb_state *mrb, const char *p, size_t len); /* mrb_str_new */
mrb_value mrb_str_new_cstr(mrb_state*, const char*);

mrb_state* mrb_open(void);
mrb_state* mrb_open_allocf(mrb_allocf);
void mrb_close(mrb_state*);
int mrb_checkstack(mrb_state*,int);

mrb_value mrb_top_self(mrb_state *);
mrb_value mrb_run(mrb_state*, struct RProc*, mrb_value);

mrb_value mrb_p(mrb_state*, mrb_value);
int mrb_obj_id(mrb_value obj);
mrb_sym mrb_to_id(mrb_state *mrb, mrb_value name);

int mrb_obj_equal(mrb_state*, mrb_value, mrb_value);
int mrb_equal(mrb_state *mrb, mrb_value obj1, mrb_value obj2);
mrb_value mrb_Integer(mrb_state *mrb, mrb_value val);
mrb_value mrb_Float(mrb_state *mrb, mrb_value val);
mrb_value mrb_inspect(mrb_state *mrb, mrb_value obj);
int mrb_eql(mrb_state *mrb, mrb_value obj1, mrb_value obj2);

void mrb_garbage_collect(mrb_state*);
void mrb_incremental_gc(mrb_state *);
int mrb_gc_arena_save(mrb_state*);
void mrb_gc_arena_restore(mrb_state*,int);
void mrb_gc_mark(mrb_state*,struct RBasic*);
#define mrb_gc_mark_value(mrb,val) do {\
  if ((val).tt >= MRB_TT_OBJECT) mrb_gc_mark((mrb), mrb_object(val));\
} while (0);
void mrb_field_write_barrier(mrb_state *, struct RBasic*, struct RBasic*);
#define mrb_field_write_barrier_value(mrb, obj, val) do{\
  if ((val.tt >= MRB_TT_OBJECT)) mrb_field_write_barrier((mrb), (obj), mrb_object(val));\
} while (0);
void mrb_write_barrier(mrb_state *, struct RBasic*);

#define MRUBY_VERSION "Rite"

#ifdef DEBUG
#undef DEBUG
#endif

#if 0
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

mrb_value mrb_check_convert_type(mrb_state *mrb, mrb_value val, mrb_int type, const char *tname, const char *method);
mrb_value mrb_any_to_s(mrb_state *mrb, mrb_value obj);
const char * mrb_obj_classname(mrb_state *mrb, mrb_value obj);
struct RClass* mrb_obj_class(mrb_state *mrb, mrb_value obj);
mrb_value mrb_class_path(mrb_state *mrb, struct RClass *c);
mrb_value mrb_convert_type(mrb_state *mrb, mrb_value val, mrb_int type, const char *tname, const char *method);
int mrb_obj_is_kind_of(mrb_state *mrb, mrb_value obj, struct RClass *c);
mrb_value mrb_obj_inspect(mrb_state *mrb, mrb_value self);
mrb_value mrb_obj_clone(mrb_state *mrb, mrb_value self);
mrb_value mrb_check_funcall(mrb_state *mrb, mrb_value recv, mrb_sym mid, int argc, mrb_value *argv);

/* need to include <ctype.h> to use these macros */
#ifndef ISPRINT
//#define ISASCII(c) isascii((int)(unsigned char)(c))
#define ISASCII(c) 1
#undef ISPRINT
#define ISPRINT(c) (ISASCII(c) && isprint((int)(unsigned char)(c)))
#define ISSPACE(c) (ISASCII(c) && isspace((int)(unsigned char)(c)))
#define ISUPPER(c) (ISASCII(c) && isupper((int)(unsigned char)(c)))
#define ISLOWER(c) (ISASCII(c) && islower((int)(unsigned char)(c)))
#define ISALNUM(c) (ISASCII(c) && isalnum((int)(unsigned char)(c)))
#define ISALPHA(c) (ISASCII(c) && isalpha((int)(unsigned char)(c)))
#define ISDIGIT(c) (ISASCII(c) && isdigit((int)(unsigned char)(c)))
#define ISXDIGIT(c) (ISASCII(c) && isxdigit((int)(unsigned char)(c)))
#endif

extern mrb_value mrb_rs;
extern mrb_value mrb_default_rs;

int mrb_block_given_p(void);
void mrb_raise(mrb_state *mrb, struct RClass *c, const char *fmt, ...);
void rb_raise(struct RClass *c, const char *fmt, ...);
void mrb_warn(const char *fmt, ...);
void mrb_warning(const char *fmt, ...);
void mrb_bug(const char *fmt, ...);

#define E_TYPE_ERROR                (mrb_class_obj_get(mrb, "TypeError"))
#define E_ARGUMENT_ERROR            (mrb_class_obj_get(mrb, "ArgumentError"))
#define E_INDEX_ERROR               (mrb_class_obj_get(mrb, "IndexError"))
#define E_RANGE_ERROR               (mrb_class_obj_get(mrb, "RangeError"))
#define E_NAME_ERROR                (mrb_class_obj_get(mrb, "NameError"))
#define E_NOMETHOD_ERROR            (mrb_class_obj_get(mrb, "NoMethodError"))
#define E_SCRIPT_ERROR              (mrb_class_obj_get(mrb, "ScriptError"))
#define E_SYNTAX_ERROR              (mrb_class_obj_get(mrb, "SyntaxError"))
#define E_LOAD_ERROR                (mrb_class_obj_get(mrb, "LoadError"))
#define E_SYSTEMCALL_ERROR          (mrb_class_obj_get(mrb, "SystemCallError"))
#define E_LOCALJUMP_ERROR           (mrb_class_obj_get(mrb, "LocalJumpError"))
#define E_REGEXP_ERROR              (mrb_class_obj_get(mrb, "RegexpError"))
#define E_ZERODIVISION_ERROR        (mrb_class_obj_get(mrb, "ZeroDivisionError"))

#define E_ENCODING_ERROR            (mrb_class_obj_get(mrb, "EncodingError"))
#define E_NOTIMP_ERROR              (mrb_class_obj_get(mrb, "NotImplementedError"))
#define E_FLOATDOMAIN_ERROR         (mrb_class_obj_get(mrb, "FloatDomainError"))

#define E_KEY_ERROR                 (mrb_class_obj_get(mrb, "KeyError"))

#define SYM2ID(x) ((x).value.sym)

#define CONST_ID_CACHE(mrb, result, str) \
  {                                 \
  static mrb_sym mrb_intern_id_cache;\
  if (!mrb_intern_id_cache)          \
    mrb_intern_id_cache = mrb_intern(mrb, str); \
    result mrb_intern_id_cache;      \
  }
#define CONST_ID(mrb, var, str) \
    do CONST_ID_CACHE(mrb, var =, str) while (0)

#define NUM2CHR_internal(x) (((mrb_type(x) == MRB_TT_STRING)&&(RSTRING_LEN(x)>=1))?\
                     RSTRING_PTR(x)[0]:(char)(mrb_fixnum_number(x)&0xff))
#ifdef __GNUC__
# define NUM2CHR(x) __extension__ ({mrb_value num2chr_x = (x); NUM2CHR_internal(num2chr_x);})
#else
/* TODO: there is no definitions of RSTRING_* here, so cannot compile.
static inline char
NUM2CHR(mrb_value x)
{
    return NUM2CHR_internal(x);
}
*/
#define NUM2CHR(x) NUM2CHR_internal(x)
#endif
mrb_value mrb_io_gets(mrb_state *mrb, mrb_value);
mrb_value mrb_io_getbyte(mrb_state *mrb, mrb_value);
mrb_value mrb_io_ungetc(mrb_state *, mrb_value, mrb_value);
mrb_value mrb_io_ungetbyte(mrb_state *mrb, mrb_value, mrb_value);
mrb_value mrb_io_close(mrb_state *mrb, mrb_value);
mrb_value mrb_io_flush(mrb_state *mrb, mrb_value);
mrb_value mrb_io_eof(mrb_value);
mrb_value mrb_io_binmode(mrb_state *mrb, mrb_value);
mrb_value mrb_io_ascii8bit_binmode(mrb_value);
mrb_value mrb_io_addstr(mrb_state *mrb, mrb_value, mrb_value);
mrb_value mrb_io_printf(mrb_state *mrb, int, mrb_value*, mrb_value);
mrb_value mrb_io_print(mrb_state *mrb, int, mrb_value*, mrb_value);
mrb_value mrb_io_puts(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value);
mrb_value mrb_io_fdopen(int, int, const char*);
mrb_value mrb_io_get_io(mrb_state *mrb, mrb_value);
mrb_value mrb_file_open(mrb_state *mrb, const char*, const char*);
mrb_value mrb_file_open_str(mrb_value, const char*);
mrb_value mrb_gets(mrb_state *mrb);

mrb_value mrb_yield(mrb_state *mrb, mrb_value v, mrb_value blk);
mrb_value mrb_yield_argv(mrb_state *mrb, mrb_value b, int argc, mrb_value *argv);
mrb_value mrb_yield_with_self(mrb_state *mrb, mrb_value b, int argc, mrb_value *argv, mrb_value self);
mrb_value mrb_class_new_instance(mrb_state *mrb, int, mrb_value*, struct RClass *);
mrb_value mrb_class_new_instance_m(mrb_state *mrb, mrb_value klass);

mrb_value mrb_exec_recursive(mrb_state *mrb, mrb_value(*)(mrb_state *, mrb_value, mrb_value, int),mrb_value,void *);

#ifndef xmalloc
#define xmalloc     malloc
#define xrealloc    realloc
#define xcalloc     calloc
#define xfree       free
#endif

void mrb_gc(void);
#define thread_debug if(0)printf

#define RUBY_VM 1 /* YARV */
#define HAVE_NATIVETHREAD
int ruby_native_thread_p(void);

#define RUBY_EVENT_NONE      0x0000
#define RUBY_EVENT_LINE      0x0001
#define RUBY_EVENT_CLASS     0x0002
#define RUBY_EVENT_END       0x0004
#define RUBY_EVENT_CALL      0x0008
#define RUBY_EVENT_RETURN    0x0010
#define RUBY_EVENT_C_CALL    0x0020
#define RUBY_EVENT_C_RETURN  0x0040
#define RUBY_EVENT_RAISE     0x0080
#define RUBY_EVENT_ALL       0xffff
#define RUBY_EVENT_VM       0x10000
#define RUBY_EVENT_SWITCH   0x20000
#define RUBY_EVENT_COVERAGE 0x40000

typedef unsigned int mrb_event_flag_t;
typedef void (*mrb_event_hook_func_t)(mrb_state *mrb, mrb_event_flag_t, mrb_value data, mrb_value, mrb_sym, mrb_value klass);

typedef struct mrb_event_hook_struct {
    mrb_event_flag_t flag;
    mrb_event_hook_func_t func;
    mrb_value data;
    struct mrb_event_hook_struct *next;
} mrb_event_hook_t;

#define RB_EVENT_HOOKS_HAVE_CALLBACK_DATA 1
void mrb_add_event_hook(mrb_state *mrb, mrb_event_hook_func_t func, mrb_event_flag_t events,
               mrb_value data);
int mrb_remove_event_hook(mrb_event_hook_func_t func);
mrb_value mrb_to_int(mrb_state *mrb, mrb_value val);
void mrb_check_type(mrb_state *mrb, mrb_value x, enum mrb_vtype t);

//#define RUBY_SETJMP(env) ${setjmp_prefix}setjmp(env${setjmp_sigmask+,0})
//#define RUBY_LONGJMP(env,val) ${setjmp_prefix}longjmp(env,val)
//#define RUBY_JMP_BUF ${setjmp_sigmask+${setjmp_prefix}}jmp_buf
#define RUBY_SETJMP(env) __builtin_setjmp(env)
#define RUBY_LONGJMP(env,val) __builtin_longjmp(env,1)//(env,val)
//#define RUBY_JMP_BUF ${setjmp_sigmask+${setjmp_prefix}}jmp_buf
#define select(n, r, w, e, t) select_large_fdset(n, r, w, e, t)

//int RUBY_SETJMP(mrb_jmpbuf_t env); /* add kusuda */
#define ruby_setjmp(env) RUBY_SETJMP(env)
#define ruby_longjmp(env,val) RUBY_LONGJMP(env,val)

#define KHASH  0
#define STHASH 1
#define BASICHASH 2
#define HASH_CLASS_METHOD BASICHASH

typedef enum call_type {
    CALL_PUBLIC,
    CALL_FCALL,
    CALL_VCALL,
    CALL_TYPE_MAX
} call_type;

/* compar.c */
void mrb_cmperr(mrb_state *mrb, mrb_value x, mrb_value y);
int mrb_cmpint(mrb_state *mrb, mrb_value val, mrb_value a, mrb_value b);

#define REALLOC_N(mrb,var,type,n) (var)=(type*)mrb_realloc(mrb, (char*)(var), sizeof(type)*(n))

#ifndef ANYARGS
# ifdef __cplusplus
#   define ANYARGS ...
# else
#   define ANYARGS
# endif
#endif
void mrb_define_alias(mrb_state *mrb, struct RClass *klass, const char *name1, const char *name2);
const char *mrb_class_name(mrb_state *mrb, struct RClass* klass);
void mrb_define_global_const(mrb_state *mrb, const char *name, mrb_value val);

mrb_value mrb_block_proc(void);
int mrb_sourceline(void);
void ruby_default_signal(int sig);
mrb_value mrb_attr_get(mrb_state *mrb, mrb_value obj, mrb_sym id);

#endif  /* MRUBY_H */
