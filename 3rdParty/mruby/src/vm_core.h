/*
** vm_core.h - RiteVM core
** 
** See Copyright Notice in mruby.h
*/

#ifndef RUBY_VM_CORE_H
#define RUBY_VM_CORE_H

#define RUBY_VM_THREAD_MODEL 2

//#include "ruby/ruby.h"
#include "st.h" /* define ANYARGS */

//#include "node.h"
//#include "debug.h"
//#include "vm_opts.h"
//#include "id.h"
#include "method.h"

#if   defined(_WIN32)
#include "thread_win32.h"
#elif defined(HAVE_PTHREAD_H)
#include "thread_pthread.h"
#else
#error "unsupported thread type"
#endif

#ifndef ENABLE_VM_OBJSPACE
#ifdef _WIN32
/*
 * TODO: object space indenpendent st_table.
 * socklist needs st_table in mrb_w32_sysinit(), before object space
 * initialization.
 * It is too early now to change st_hash_type, since it breaks binary
 * compatibility.
 */
#define ENABLE_VM_OBJSPACE 0
#else
#define ENABLE_VM_OBJSPACE 1
#endif
#endif

#include <setjmp.h>
#include <signal.h>

//#ifndef NSIG
//# define NSIG (_SIGMAX + 1)      /* For QNX */
//#endif

//#define RUBY_NSIG NSIG

#ifdef HAVE_STDARG_PROTOTYPES
#include <stdarg.h>
#define va_init_list(a,b) va_start(a,b)
#else
#include <varargs.h>
#define va_init_list(a,b) va_start(a)
#endif

#if defined(SIGSEGV) && defined(HAVE_SIGALTSTACK) && defined(SA_SIGINFO) && !defined(__NetBSD__)
#define USE_SIGALTSTACK
#endif

/*****************/
/* configuration */
/*****************/

/* gcc ver. check */
#if defined(__GNUC__) && __GNUC__ >= 2

#if OPT_TOKEN_THREADED_CODE
#if OPT_DIRECT_THREADED_CODE
#undef OPT_DIRECT_THREADED_CODE
#endif
#endif

#else /* defined(__GNUC__) && __GNUC__ >= 2 */

/* disable threaded code options */
#if OPT_DIRECT_THREADED_CODE
#undef OPT_DIRECT_THREADED_CODE
#endif
#if OPT_TOKEN_THREADED_CODE
#undef OPT_TOKEN_THREADED_CODE
#endif
#endif

/* call threaded code */
#if    OPT_CALL_THREADED_CODE
#if    OPT_DIRECT_THREADED_CODE
#undef OPT_DIRECT_THREADED_CODE
#endif /* OPT_DIRECT_THREADED_CODE */
#if    OPT_STACK_CACHING
#undef OPT_STACK_CACHING
#endif /* OPT_STACK_CACHING */
#endif /* OPT_CALL_THREADED_CODE */

/* likely */
#if __GNUC__ >= 3
#define LIKELY(x)   (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else /* __GNUC__ >= 3 */
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif /* __GNUC__ >= 3 */

#if __GNUC__ >= 3
#define UNINITIALIZED_VAR(x) x = x
#else
#define UNINITIALIZED_VAR(x) x
#endif

typedef unsigned long mrb_num_t;

/* iseq data type */

struct iseq_compile_data_ensure_node_stack;

typedef struct mrb_compile_option_struct {
    int inline_const_cache;
    int peephole_optimization;
    int tailcall_optimization;
    int specialized_instruction;
    int operands_unification;
    int instructions_unification;
    int stack_caching;
    int trace_instruction;
    int debug_level;
} mrb_compile_option_t;

struct iseq_inline_cache_entry {
	mrb_value ic_vmstat;
	mrb_value ic_class;
    union {
	mrb_value value;
	mrb_method_entry_t *method;
	long index;
    } ic_value;
};

#if 1
#define GetCoreDataFromValue(obj, type, ptr) do { \
    ptr = (type*)DATA_PTR(obj); \
} while (0)
#else
#define GetCoreDataFromValue(obj, type, ptr) Data_Get_Struct(obj, type, ptr)
#endif

#define GetISeqPtr(obj, ptr) \
  GetCoreDataFromValue(obj, mrb_iseq_t, ptr)

struct mrb_iseq_struct;

//enum ruby_special_exceptions {
//    ruby_error_reenter,
//    ruby_error_nomemory,
//    ruby_error_sysstack,
//    ruby_special_error_count
//};

#define GetVMPtr(obj, ptr) \
  GetCoreDataFromValue(obj, mrb_vm_t, ptr)

#if defined(ENABLE_VM_OBJSPACE) && ENABLE_VM_OBJSPACE
struct mrb_objspace;
void mrb_objspace_free(struct mrb_objspace *);
#endif

typedef struct mrb_block_struct {
    mrb_value self;			/* share with method frame if it's only block */
    mrb_value *lfp;			/* share with method frame if it's only block */
    mrb_value *dfp;			/* share with method frame if it's only block */
    mrb_iseq_t *iseq;
    mrb_value proc;
} mrb_block_t;

#define GetThreadPtr(obj, ptr) \
  GetCoreDataFromValue(obj, mrb_thread_t, ptr)

//typedef RUBY_JMP_BUF mrb_jmpbuf_t; /* kusuda */
#define mrb_jmpbuf_t void*   /* kusuda */

struct mrb_vm_protect_tag {
    struct mrb_vm_protect_tag *prev;
};

#define RUBY_VM_VALUE_CACHE_SIZE 0x1000
#define USE_VALUE_CACHE 0

struct mrb_mutex_struct;


/* iseq.c */
mrb_value mrb_iseq_new(NODE*, mrb_value, mrb_value, mrb_value, mrb_value, mrb_value);
mrb_value mrb_iseq_new_top(NODE *node, mrb_value name, mrb_value filename, mrb_value filepath, mrb_value parent);
mrb_value mrb_iseq_new_main(NODE *node, mrb_value filename, mrb_value filepath);
mrb_value mrb_iseq_new_with_bopt(NODE*, mrb_value, mrb_value, mrb_value, mrb_value, mrb_value, mrb_value, mrb_value);
mrb_value mrb_iseq_new_with_opt(NODE*, mrb_value, mrb_value, mrb_value, mrb_value, mrb_value, mrb_value, const mrb_compile_option_t*);
mrb_value mrb_iseq_compile(mrb_value src, mrb_value file, mrb_value line);
mrb_value mrb_iseq_disasm(mrb_value self);
int mrb_iseq_disasm_insn(mrb_value str, mrb_value *iseqval, size_t pos, mrb_iseq_t *iseq, mrb_value child);
const char *ruby_node_name(int node);
int mrb_iseq_first_lineno(mrb_iseq_t *iseq);

RUBY_EXTERN mrb_value mrb_cISeq;
RUBY_EXTERN mrb_value mrb_cRubyVM;
RUBY_EXTERN mrb_value mrb_cEnv;
RUBY_EXTERN mrb_value mrb_mRubyVMFrozenCore;

/* each thread has this size stack : 128KB */
#define RUBY_VM_THREAD_STACK_SIZE (128 * 1024)

#define GetProcPtr(obj, ptr) \
  GetCoreDataFromValue(obj, mrb_proc_t, ptr)

typedef struct {
    mrb_block_t block;

    mrb_value envval;		/* for GC mark */
    mrb_value blockprocval;
    int safe_level;
    int is_from_method;
    int is_lambda;
} mrb_proc_t;

#define GetEnvPtr(obj, ptr) \
  GetCoreDataFromValue(obj, mrb_env_t, ptr)

typedef struct {
    mrb_value *env;
    int env_size;
    int local_size;
    mrb_value prev_envval;		/* for GC mark */
    mrb_block_t block;
} mrb_env_t;

//#define GetBindingPtr(obj, ptr) 
//  GetCoreDataFromValue(obj, mrb_binding_t, ptr)

//typedef struct {
//    mrb_value env;
//    mrb_value filename;
//    unsigned short line_no;
//} mrb_binding_t;

/* used by compile time and send insn */
#define VM_CALL_ARGS_SPLAT_BIT     (0x01 << 1)
#define VM_CALL_ARGS_BLOCKARG_BIT  (0x01 << 2)
#define VM_CALL_FCALL_BIT          (0x01 << 3)
#define VM_CALL_VCALL_BIT          (0x01 << 4)
#define VM_CALL_TAILCALL_BIT       (0x01 << 5)
#define VM_CALL_TAILRECURSION_BIT  (0x01 << 6)
#define VM_CALL_SUPER_BIT          (0x01 << 7)
#define VM_CALL_OPT_SEND_BIT       (0x01 << 8)

#define VM_SPECIAL_OBJECT_VMCORE       0x01
#define VM_SPECIAL_OBJECT_CBASE        0x02
#define VM_SPECIAL_OBJECT_CONST_BASE  0x03

#define VM_FRAME_MAGIC_METHOD 0x11
#define VM_FRAME_MAGIC_BLOCK  0x21
#define VM_FRAME_MAGIC_CLASS  0x31
#define VM_FRAME_MAGIC_TOP    0x41
#define VM_FRAME_MAGIC_FINISH 0x51
#define VM_FRAME_MAGIC_CFUNC  0x61
#define VM_FRAME_MAGIC_PROC   0x71
#define VM_FRAME_MAGIC_IFUNC  0x81
#define VM_FRAME_MAGIC_EVAL   0x91
#define VM_FRAME_MAGIC_LAMBDA 0xa1
#define VM_FRAME_MAGIC_MASK_BITS   8
#define VM_FRAME_MAGIC_MASK   (~(~0<<VM_FRAME_MAGIC_MASK_BITS))

#define VM_FRAME_TYPE(cfp) ((cfp)->flag & VM_FRAME_MAGIC_MASK)

/* other frame flag */
#define VM_FRAME_FLAG_PASSED 0x0100

#define RUBYVM_CFUNC_FRAME_P(cfp) \
  (VM_FRAME_TYPE(cfp) == VM_FRAME_MAGIC_CFUNC)

/* inline cache */
typedef struct iseq_inline_cache_entry *IC;

extern mrb_value ruby_vm_global_state_version;

#define GET_VM_STATE_VERSION() (ruby_vm_global_state_version)
#define INC_VM_STATE_VERSION() \
  (ruby_vm_global_state_version = (ruby_vm_global_state_version+1) & 0x8fffffff)
void mrb_vm_change_state(void);

typedef mrb_value CDHASH;

#define GC_GUARDED_PTR(p)     ((mrb_value)((mrb_value)(p) | 0x01))
#define GC_GUARDED_PTR_REF(p) ((void *)(((mrb_value)p) & ~0x03))
#define GC_GUARDED_PTR_P(p)   (((mrb_value)p) & 0x01)

#define RUBY_VM_PREVIOUS_CONTROL_FRAME(cfp) (cfp+1)
#define RUBY_VM_NEXT_CONTROL_FRAME(cfp) (cfp-1)
#define RUBY_VM_END_CONTROL_FRAME(th) \
  ((mrb_control_frame_t *)((th)->stack + (th)->stack_size))
#define RUBY_VM_VALID_CONTROL_FRAME_P(cfp, ecfp) \
  ((void *)(ecfp) > (void *)(cfp))
#define RUBY_VM_CONTROL_FRAME_STACK_OVERFLOW_P(th, cfp) \
  (!RUBY_VM_VALID_CONTROL_FRAME_P((cfp), RUBY_VM_END_CONTROL_FRAME(th)))

#define RUBY_VM_IFUNC_P(ptr)        (BUILTIN_TYPE(ptr) == T_NODE)
#define RUBY_VM_NORMAL_ISEQ_P(ptr) \
  (ptr && !RUBY_VM_IFUNC_P(ptr))

#define RUBY_VM_GET_BLOCK_PTR_IN_CFP(cfp) ((mrb_block_t *)(&(cfp)->self))
#define RUBY_VM_GET_CFP_FROM_BLOCK_PTR(b) \
  ((mrb_control_frame_t *)((mrb_value *)(b) - 5))

/* VM related object allocate functions */
//mrb_value mrb_thread_alloc(mrb_value klass);
mrb_value mrb_proc_alloc(mrb_value klass);

/* for debug */
extern void mrb_vmdebug_stack_dump_raw(mrb_thread_t *, mrb_control_frame_t *);
#define SDR() mrb_vmdebug_stack_dump_raw(GET_THREAD(), GET_THREAD()->cfp)
#define SDR2(cfp) mrb_vmdebug_stack_dump_raw(GET_THREAD(), (cfp))
void mrb_vm_bugreport(void);

/* functions about thread/vm execution */
mrb_value mrb_iseq_eval(mrb_value iseqval);
mrb_value mrb_iseq_eval_main(mrb_value iseqval);
void mrb_enable_interrupt(void);
void mrb_disable_interrupt(void);
//int mrb_thread_method_id_and_class(mrb_thread_t *th, mrb_sym *idp, mrb_value *klassp);

mrb_value mrb_vm_invoke_proc(mrb_thread_t *th, mrb_proc_t *proc, mrb_value self,
			int argc, const mrb_value *argv, const mrb_block_t *blockptr);
mrb_value mrb_vm_make_proc(mrb_thread_t *th, const mrb_block_t *block, mrb_value klass);
mrb_value mrb_vm_make_env_object(mrb_thread_t *th, mrb_control_frame_t *cfp);

//void mrb_thread_start_timer_thread(void);
//void mrb_thread_stop_timer_thread(void);
//void mrb_thread_reset_timer_thread(void);
//void *mrb_thread_call_with_gvl(void *(*func)(void *), void *data1);
int ruby_thread_has_gvl_p(void);
mrb_value mrb_make_backtrace(void);
typedef int mrb_backtrace_iter_func(void *, mrb_value, int, mrb_value);
int mrb_backtrace_each(mrb_backtrace_iter_func *iter, void *arg);
//mrb_control_frame_t *mrb_vm_get_ruby_level_next_cfp(mrb_thread_t *th, mrb_control_frame_t *cfp);
int mrb_vm_get_sourceline(const mrb_control_frame_t *);
mrb_value mrb_name_err_mesg_new(mrb_value obj, mrb_value mesg, mrb_value recv, mrb_value method);

NOINLINE(void mrb_gc_save_machine_context(mrb_thread_t *));

//#define sysstack_error GET_VM()->special_exceptions[ruby_error_sysstack]

mrb_value mrb_str_resurrect(mrb_value str);
mrb_value mrb_ary_resurrect(mrb_value ary);

/* for thread */

#if RUBY_VM_THREAD_MODEL == 2
RUBY_EXTERN mrb_thread_t *ruby_current_thread;
extern mrb_vm_t *ruby_current_vm;

#define GET_VM() ruby_current_vm
#define GET_THREAD() ruby_current_thread
#define mrb_thread_set_current_raw(th) (void)(ruby_current_thread = (th))
#define mrb_thread_set_current(th) do { \
    mrb_thread_set_current_raw(th); \
    th->vm->running_thread = th; \
} while (0)

#else
#error "unsupported thread model"
#endif

#define RUBY_VM_SET_INTERRUPT(th) ((th)->interrupt_flag |= 0x02)
#define RUBY_VM_SET_TIMER_INTERRUPT(th) ((th)->interrupt_flag |= 0x01)
#define RUBY_VM_SET_FINALIZER_INTERRUPT(th) ((th)->interrupt_flag |= 0x04)
#define RUBY_VM_INTERRUPTED(th) ((th)->interrupt_flag & 0x02)

void mrb_threadptr_check_signal(mrb_thread_t *mth);
//void mrb_threadptr_signal_raise(mrb_thread_t *th, int sig);
void mrb_threadptr_signal_exit(mrb_state *mrb, mrb_thread_t *th);
//void mrb_threadptr_execute_interrupts(mrb_thread_t *);

void mrb_thread_lock_unlock(mrb_thread_lock_t *);
void mrb_thread_lock_destroy(mrb_thread_lock_t *);

//#define RUBY_VM_CHECK_INTS_TH(th) do { \
//  if (UNLIKELY(th->interrupt_flag)) { \
//    mrb_threadptr_execute_interrupts(th); \
//  } \
//} while (0)

//#define RUBY_VM_CHECK_INTS() \
//  RUBY_VM_CHECK_INTS_TH(GET_THREAD())

/* tracer */
//void
//mrb_threadptr_exec_event_hooks(mrb_thread_t *th, mrb_event_flag_t flag, mrb_value self, mrb_sym id, mrb_value klass);
#if 0
#define EXEC_EVENT_HOOK(th, flag, self, id, klass) do { \
    mrb_event_flag_t wait_event__ = th->event_flags; \
    if (UNLIKELY(wait_event__)) { \
	if (wait_event__ & (flag | RUBY_EVENT_VM)) { \
	    mrb_threadptr_exec_event_hooks(th, flag, self, id, klass); \
	} \
    } \
} while (0)
#endif
#endif /* RUBY_VM_CORE_H */
