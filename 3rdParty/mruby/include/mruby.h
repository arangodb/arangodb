/*
** mruby - An embeddable Ruby implementation
**
** Copyright (c) mruby developers 2010-2013
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

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include "mrbconf.h"
#include "mruby/value.h"

typedef uint32_t mrb_code;
typedef uint32_t mrb_aspec;

struct mrb_state;

typedef void* (*mrb_allocf) (struct mrb_state *mrb, void*, size_t, void *ud);

#ifndef MRB_ARENA_SIZE
#define MRB_ARENA_SIZE 100
#endif

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

enum mrb_fiber_state {
  MRB_FIBER_CREATED = 0,
  MRB_FIBER_RUNNING,
  MRB_FIBER_RESUMED,
  MRB_FIBER_TERMINATED,
};

struct mrb_context {
  struct mrb_context *prev;

  mrb_value *stack;                       /* stack of virtual machine */
  mrb_value *stbase, *stend;

  mrb_callinfo *ci;
  mrb_callinfo *cibase, *ciend;

  mrb_code **rescue;                      /* exception handler stack */
  int rsize;
  struct RProc **ensure;                  /* ensure handler stack */
  int esize;

  uint8_t status;
  struct RFiber *fib;
};

enum gc_state {
  GC_STATE_NONE = 0,
  GC_STATE_MARK,
  GC_STATE_SWEEP
};

typedef struct mrb_state {
  void *jmp;

  mrb_allocf allocf;                      /* memory allocation function */

  struct mrb_context *c;
  struct mrb_context *root_c;

  struct RObject *exc;                    /* exception */
  struct iv_tbl *globals;                 /* global variable table */
  struct mrb_irep **irep;                 /* program data array */
  size_t irep_len, irep_capa;

  struct RObject *top_self;
  struct RClass *object_class;            /* Object class */
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

  struct heap_page *heaps;                /* heaps for GC */
  struct heap_page *sweeps;
  struct heap_page *free_heaps;
  size_t live; /* count of live objects */
  struct RBasic *arena[MRB_ARENA_SIZE];   /* GC protection array */
  int arena_idx;

  enum gc_state gc_state; /* state of gc */
  int current_white_part; /* make white object by white_part */
  struct RBasic *gray_list; /* list of gray objects to be traversed incrementally */
  struct RBasic *atomic_gray_list; /* list of objects to be traversed atomically */
  size_t gc_live_after_mark;
  size_t gc_threshold;
  int gc_interval_ratio;
  int gc_step_ratio;
  mrb_bool gc_disabled:1;
  mrb_bool gc_full:1;
  mrb_bool is_generational_gc_mode:1;
  mrb_bool out_of_memory:1;
  size_t majorgc_old_threshold;
  struct alloca_header *mems;

  mrb_sym symidx;
  struct kh_n2s *name2sym;      /* symbol table */

#ifdef ENABLE_DEBUG
  void (*code_fetch_hook)(struct mrb_state* mrb, struct mrb_irep *irep, mrb_code *pc, mrb_value *regs);
#endif

  struct RClass *eException_class;
  struct RClass *eStandardError_class;

  void *ud; /* auxiliary data */
} mrb_state;

typedef mrb_value (*mrb_func_t)(mrb_state *mrb, mrb_value);
struct RClass *mrb_define_class(mrb_state *, const char*, struct RClass*);
struct RClass *mrb_define_module(mrb_state *, const char*);
mrb_value mrb_singleton_class(mrb_state*, mrb_value);
void mrb_include_module(mrb_state*, struct RClass*, struct RClass*);

void mrb_define_method(mrb_state*, struct RClass*, const char*, mrb_func_t, mrb_aspec);
void mrb_define_class_method(mrb_state *, struct RClass *, const char *, mrb_func_t, mrb_aspec);
void mrb_define_singleton_method(mrb_state*, struct RObject*, const char*, mrb_func_t, mrb_aspec);
void mrb_define_module_function(mrb_state*, struct RClass*, const char*, mrb_func_t, mrb_aspec);
void mrb_define_const(mrb_state*, struct RClass*, const char *name, mrb_value);
void mrb_undef_method(mrb_state*, struct RClass*, const char*);
void mrb_undef_class_method(mrb_state*, struct RClass*, const char*);
mrb_value mrb_obj_new(mrb_state *mrb, struct RClass *c, int argc, mrb_value *argv);
#define mrb_class_new_instance(mrb,argc,argv,c) mrb_obj_new(mrb,c,argc,argv)
mrb_value mrb_class_obj_new(mrb_state *mrb, struct RClass *c, int argc, mrb_value *argv);
mrb_value mrb_instance_new(mrb_state *mrb, mrb_value cv);
struct RClass * mrb_class_new(mrb_state *mrb, struct RClass *super);
struct RClass * mrb_module_new(mrb_state *mrb);
mrb_bool mrb_class_defined(mrb_state *mrb, const char *name);
struct RClass * mrb_class_get(mrb_state *mrb, const char *name);
struct RClass * mrb_class_get_under(mrb_state *mrb, struct RClass *outer, const char *name);

mrb_value mrb_obj_dup(mrb_state *mrb, mrb_value obj);
mrb_value mrb_check_to_integer(mrb_state *mrb, mrb_value val, const char *method);
mrb_bool mrb_obj_respond_to(struct RClass* c, mrb_sym mid);
struct RClass * mrb_define_class_under(mrb_state *mrb, struct RClass *outer, const char *name, struct RClass *super);
struct RClass * mrb_define_module_under(mrb_state *mrb, struct RClass *outer, const char *name);

/* required arguments */
#define MRB_ARGS_REQ(n)     ((mrb_aspec)((n)&0x1f) << 18)
/* optional arguments */
#define MRB_ARGS_OPT(n)     ((mrb_aspec)((n)&0x1f) << 13)
/* mandatory and optinal arguments */
#define MRB_ARGS_ARG(n1,n2)   (MRB_ARGS_REQ(n1)|MRB_ARGS_OPT(n2))

/* rest argument */
#define MRB_ARGS_REST()     ((mrb_aspec)(1 << 12))
/* required arguments after rest */
#define MRB_ARGS_POST(n)    ((mrb_aspec)((n)&0x1f) << 7)
/* keyword arguments (n of keys, kdict) */
#define MRB_ARGS_KEY(n1,n2) ((mrb_aspec)((((n1)&0x1f) << 2) | ((n2)?(1<<1):0)))
/* block argument */
#define MRB_ARGS_BLOCK()    ((mrb_aspec)1)

/* accept any number of arguments */
#define MRB_ARGS_ANY()      ARGS_REST()
/* accept no arguments */
#define MRB_ARGS_NONE()     ((mrb_aspec)0)

/* compatibility macros; will be removed */
#define ARGS_REQ(n)         MRB_ARGS_REQ(n)
#define ARGS_OPT(n)         MRB_ARGS_OPT(n)
#define ARGS_REST()         MRB_ARGS_REST()
#define ARGS_POST(n)        MRB_ARGS_POST()
#define ARGS_KEY(n1,n2)     MRB_ARGS_KEY(n1,n2)
#define ARGS_BLOCK()        MRB_ARGS_BLOCK()
#define ARGS_ANY()          MRB_ARGS_ANY()
#define ARGS_NONE()         MRB_ARGS_NONE()

int mrb_get_args(mrb_state *mrb, const char *format, ...);

mrb_value mrb_funcall(mrb_state*, mrb_value, const char*, int,...);
mrb_value mrb_funcall_argv(mrb_state*, mrb_value, mrb_sym, int, mrb_value*);
mrb_value mrb_funcall_with_block(mrb_state*, mrb_value, mrb_sym, int, mrb_value*, mrb_value);
mrb_sym mrb_intern_cstr(mrb_state*,const char*);
mrb_sym mrb_intern2(mrb_state*,const char*,size_t);
mrb_sym mrb_intern_str(mrb_state*,mrb_value);
mrb_value mrb_check_intern_cstr(mrb_state*,const char*);
mrb_value mrb_check_intern(mrb_state*,const char*,size_t);
mrb_value mrb_check_intern_str(mrb_state*,mrb_value);
const char *mrb_sym2name(mrb_state*,mrb_sym);
const char *mrb_sym2name_len(mrb_state*,mrb_sym,size_t*);
mrb_value mrb_sym2str(mrb_state*,mrb_sym);
mrb_value mrb_str_format(mrb_state *, int, const mrb_value *, mrb_value);

/* For backward compatibility. */
static inline
mrb_sym mrb_intern(mrb_state *mrb,const char *cstr)
{
  return mrb_intern_cstr(mrb, cstr);
}

void *mrb_malloc(mrb_state*, size_t);         /* raise RuntimeError if no mem */
void *mrb_calloc(mrb_state*, size_t, size_t); /* ditto */
void *mrb_realloc(mrb_state*, void*, size_t); /* ditto */
void *mrb_realloc_simple(mrb_state*, void*, size_t); /* return NULL if no memory available */
void *mrb_malloc_simple(mrb_state*, size_t);  /* return NULL if no memory available */
struct RBasic *mrb_obj_alloc(mrb_state*, enum mrb_vtype, struct RClass*);
void mrb_free(mrb_state*, void*);

mrb_value mrb_str_new(mrb_state *mrb, const char *p, size_t len);
mrb_value mrb_str_new_cstr(mrb_state*, const char*);
mrb_value mrb_str_new_static(mrb_state *mrb, const char *p, size_t len);

mrb_state* mrb_open(void);
mrb_state* mrb_open_allocf(mrb_allocf, void *ud);
void mrb_irep_free(mrb_state*, struct mrb_irep*);
void mrb_close(mrb_state*);

mrb_value mrb_top_self(mrb_state *);
mrb_value mrb_run(mrb_state*, struct RProc*, mrb_value);

void mrb_p(mrb_state*, mrb_value);
mrb_int mrb_obj_id(mrb_value obj);
mrb_sym mrb_obj_to_sym(mrb_state *mrb, mrb_value name);

mrb_bool mrb_obj_eq(mrb_state*, mrb_value, mrb_value);
mrb_bool mrb_obj_equal(mrb_state*, mrb_value, mrb_value);
mrb_bool mrb_equal(mrb_state *mrb, mrb_value obj1, mrb_value obj2);
mrb_value mrb_Integer(mrb_state *mrb, mrb_value val);
mrb_value mrb_Float(mrb_state *mrb, mrb_value val);
mrb_value mrb_inspect(mrb_state *mrb, mrb_value obj);
mrb_bool mrb_eql(mrb_state *mrb, mrb_value obj1, mrb_value obj2);

void mrb_garbage_collect(mrb_state*);
void mrb_full_gc(mrb_state*);
void mrb_incremental_gc(mrb_state *);
int mrb_gc_arena_save(mrb_state*);
void mrb_gc_arena_restore(mrb_state*,int);
void mrb_gc_mark(mrb_state*,struct RBasic*);
#define mrb_gc_mark_value(mrb,val) do {\
  if (mrb_type(val) >= MRB_TT_HAS_BASIC) mrb_gc_mark((mrb), mrb_basic_ptr(val));\
} while (0)
void mrb_field_write_barrier(mrb_state *, struct RBasic*, struct RBasic*);
#define mrb_field_write_barrier_value(mrb, obj, val) do{\
  if ((val.tt >= MRB_TT_HAS_BASIC)) mrb_field_write_barrier((mrb), (obj), mrb_basic_ptr(val));\
} while (0)
void mrb_write_barrier(mrb_state *, struct RBasic*);

mrb_value mrb_check_convert_type(mrb_state *mrb, mrb_value val, enum mrb_vtype type, const char *tname, const char *method);
mrb_value mrb_any_to_s(mrb_state *mrb, mrb_value obj);
const char * mrb_obj_classname(mrb_state *mrb, mrb_value obj);
struct RClass* mrb_obj_class(mrb_state *mrb, mrb_value obj);
mrb_value mrb_class_path(mrb_state *mrb, struct RClass *c);
mrb_value mrb_convert_type(mrb_state *mrb, mrb_value val, enum mrb_vtype type, const char *tname, const char *method);
mrb_bool mrb_obj_is_kind_of(mrb_state *mrb, mrb_value obj, struct RClass *c);
mrb_value mrb_obj_inspect(mrb_state *mrb, mrb_value self);
mrb_value mrb_obj_clone(mrb_state *mrb, mrb_value self);

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
#define TOUPPER(c) (ISASCII(c) ? toupper((int)(unsigned char)(c)) : (c))
#define TOLOWER(c) (ISASCII(c) ? tolower((int)(unsigned char)(c)) : (c))
#endif

mrb_value mrb_exc_new(mrb_state *mrb, struct RClass *c, const char *ptr, long len);
void mrb_exc_raise(mrb_state *mrb, mrb_value exc);

void mrb_raise(mrb_state *mrb, struct RClass *c, const char *msg);
void mrb_raisef(mrb_state *mrb, struct RClass *c, const char *fmt, ...);
void mrb_name_error(mrb_state *mrb, mrb_sym id, const char *fmt, ...);
void mrb_warn(mrb_state *mrb, const char *fmt, ...);
void mrb_bug(mrb_state *mrb, const char *fmt, ...);
void mrb_print_backtrace(mrb_state *mrb);
void mrb_print_error(mrb_state *mrb);

/* macros to get typical exception objects
   note:
   + those E_* macros requires mrb_state* variable named mrb.
   + exception objects obtained from those macros are local to mrb
*/
#define E_RUNTIME_ERROR             (mrb_class_get(mrb, "RuntimeError"))
#define E_TYPE_ERROR                (mrb_class_get(mrb, "TypeError"))
#define E_ARGUMENT_ERROR            (mrb_class_get(mrb, "ArgumentError"))
#define E_INDEX_ERROR               (mrb_class_get(mrb, "IndexError"))
#define E_RANGE_ERROR               (mrb_class_get(mrb, "RangeError"))
#define E_NAME_ERROR                (mrb_class_get(mrb, "NameError"))
#define E_NOMETHOD_ERROR            (mrb_class_get(mrb, "NoMethodError"))
#define E_SCRIPT_ERROR              (mrb_class_get(mrb, "ScriptError"))
#define E_SYNTAX_ERROR              (mrb_class_get(mrb, "SyntaxError"))
#define E_LOCALJUMP_ERROR           (mrb_class_get(mrb, "LocalJumpError"))
#define E_REGEXP_ERROR              (mrb_class_get(mrb, "RegexpError"))

#define E_NOTIMP_ERROR              (mrb_class_get(mrb, "NotImplementedError"))
#define E_FLOATDOMAIN_ERROR         (mrb_class_get(mrb, "FloatDomainError"))

#define E_KEY_ERROR                 (mrb_class_get(mrb, "KeyError"))

mrb_value mrb_yield(mrb_state *mrb, mrb_value b, mrb_value arg);
mrb_value mrb_yield_argv(mrb_state *mrb, mrb_value b, int argc, mrb_value *argv);

void mrb_gc_protect(mrb_state *mrb, mrb_value obj);
mrb_value mrb_to_int(mrb_state *mrb, mrb_value val);
void mrb_check_type(mrb_state *mrb, mrb_value x, enum mrb_vtype t);

typedef enum call_type {
    CALL_PUBLIC,
    CALL_FCALL,
    CALL_VCALL,
    CALL_TYPE_MAX
} call_type;

void mrb_define_alias(mrb_state *mrb, struct RClass *klass, const char *name1, const char *name2);
const char *mrb_class_name(mrb_state *mrb, struct RClass* klass);
void mrb_define_global_const(mrb_state *mrb, const char *name, mrb_value val);

mrb_value mrb_block_proc(void);
mrb_value mrb_attr_get(mrb_state *mrb, mrb_value obj, mrb_sym id);

mrb_bool mrb_respond_to(mrb_state *mrb, mrb_value obj, mrb_sym mid);
mrb_bool mrb_obj_is_instance_of(mrb_state *mrb, mrb_value obj, struct RClass* c);

/* memory pool implementation */
typedef struct mrb_pool mrb_pool;
struct mrb_pool* mrb_pool_open(mrb_state*);
void mrb_pool_close(struct mrb_pool*);
void* mrb_pool_alloc(struct mrb_pool*, size_t);
void* mrb_pool_realloc(struct mrb_pool*, void*, size_t oldlen, size_t newlen);
mrb_bool mrb_pool_can_realloc(struct mrb_pool*, void*, size_t);
void* mrb_alloca(mrb_state *mrb, size_t);

#ifdef MRB_DEBUG
#include <assert.h>
#define mrb_assert(p) assert(p)
#else
#define mrb_assert(p) ((void)0)
#endif

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* MRUBY_H */
