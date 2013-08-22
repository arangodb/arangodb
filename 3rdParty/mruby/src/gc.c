/*
** gc.c - garbage collector for mruby
**
** See Copyright Notice in mruby.h
*/

#ifndef SIZE_MAX
 /* Some versions of VC++
  * has SIZE_MAX in stdint.h
  */
# include <limits.h>
#endif
#include <string.h>
#include <stdlib.h>
#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/hash.h"
#include "mruby/proc.h"
#include "mruby/range.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/gc.h"

/*
  = Tri-color Incremental Garbage Collection

  mruby's GC is Tri-color Incremental GC with Mark & Sweep.
  Algorithm details are omitted.
  Instead, the part about the implementation described below.

  == Object's Color

  Each object to be painted in three colors.

    * White - Unmarked.
    * Gray - Marked, But the child objects are unmarked.
    * Black - Marked, the child objects are also marked.

  == Two White Types

  There're two white color types in a flip-flop fassion: White-A and White-B,
  which respectively represent the Current White color (the newly allocated
  objects in the current GC cycle) and the Sweep Target White color (the
  dead objects to be swept).

  A and B will be switched just at the beginning of the next GC cycle. At
  that time, all the dead objects have been swept, while the newly created
  objects in the current GC cycle which finally remains White are now
  regarded as dead objects. Instead of traversing all the White-A objects and
  paint them as White-B, just switch the meaning of White-A and White-B would
  be much cheaper.

  As a result, the objects we sweep in the current GC cycle are always
  left from the previous GC cycle. This allows us to sweep objects
  incrementally, without the disturbance of the newly created objects.

  == Execution Timing

  GC Execution Time and Each step interval are decided by live objects count.
  List of Adjustment API:

    * gc_interval_ratio_set
    * gc_step_ratio_set

  For details, see the comments for each function.

  == Write Barrier

  mruby implementer, C extension library writer must write a write
  barrier when writing a pointer to an object on object's field.
  Two different write barrier:

    * mrb_field_write_barrier
    * mrb_write_barrier

  == Generational Mode

  mruby's GC offers an Generational Mode while re-using the tri-color GC
  infrastructure. It will treat the Black objects as Old objects after each
  sweep phase, instead of paint them to White. The key idea are still same as
  the traditional generational GC:

    * Minor GC - just traverse the Young objects (Gray objects) in the mark
                 phase, then only sweep the newly created objects, and leave
                 the Old objects live.

    * Major GC - same as a full regular GC cycle.

  the difference between "tranditional" generational GC is that, the major GC
  in mruby is triggered incrementally in a tri-color manner.


  For details, see the comments for each function.

*/

struct free_obj {
  MRB_OBJECT_HEADER;
  struct RBasic *next;
};

typedef struct {
  union {
    struct free_obj free;
    struct RBasic basic;
    struct RObject object;
    struct RClass klass;
    struct RString string;
    struct RArray array;
    struct RHash hash;
    struct RRange range;
    struct RData data;
    struct RProc proc;
  } as;
} RVALUE;

#ifdef GC_PROFILE
#include <stdio.h>
#include <sys/time.h>

static double program_invoke_time = 0;
static double gc_time = 0;
static double gc_total_time = 0;

static double
gettimeofday_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

#define GC_INVOKE_TIME_REPORT(with) do {\
  fprintf(stderr, "%s\n", with);\
  fprintf(stderr, "gc_invoke: %19.3f\n", gettimeofday_time() - program_invoke_time);\
  fprintf(stderr, "is_generational: %d\n", is_generational(mrb));\
  fprintf(stderr, "is_major_gc: %d\n", is_major_gc(mrb));\
} while(0)

#define GC_TIME_START do {\
  gc_time = gettimeofday_time();\
} while(0)

#define GC_TIME_STOP_AND_REPORT do {\
  gc_time = gettimeofday_time() - gc_time;\
  gc_total_time += gc_time;\
  fprintf(stderr, "gc_state: %d\n", mrb->gc_state);\
  fprintf(stderr, "live: %zu\n", mrb->live);\
  fprintf(stderr, "majorgc_old_threshold: %zu\n", mrb->majorgc_old_threshold);\
  fprintf(stderr, "gc_threshold: %zu\n", mrb->gc_threshold);\
  fprintf(stderr, "gc_time: %30.20f\n", gc_time);\
  fprintf(stderr, "gc_total_time: %30.20f\n\n", gc_total_time);\
} while(0)
#else
#define GC_INVOKE_TIME_REPORT(s)
#define GC_TIME_START
#define GC_TIME_STOP_AND_REPORT
#endif

#ifdef GC_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#define GC_STEP_SIZE 1024


void*
mrb_realloc_simple(mrb_state *mrb, void *p,  size_t len)
{
  void *p2;

  p2 = (mrb->allocf)(mrb, p, len, mrb->ud);
  if (!p2 && len > 0 && mrb->heaps) {
    mrb_full_gc(mrb);
    p2 = (mrb->allocf)(mrb, p, len, mrb->ud);
  }

  return p2;
}


void*
mrb_realloc(mrb_state *mrb, void *p, size_t len)
{
  void *p2;

  p2 = mrb_realloc_simple(mrb, p, len);
  if (!p2 && len) {
    if (mrb->out_of_memory) {
      /* mrb_panic(mrb); */
    }
    else {
      mrb->out_of_memory = TRUE;
      mrb_raise(mrb, E_RUNTIME_ERROR, "Out of memory");
    }
  }
  else {
    mrb->out_of_memory = FALSE;
  }

  return p2;
}

void*
mrb_malloc(mrb_state *mrb, size_t len)
{
  return mrb_realloc(mrb, 0, len);
}

void*
mrb_malloc_simple(mrb_state *mrb, size_t len)
{
  return mrb_realloc_simple(mrb, 0, len);
}

void*
mrb_calloc(mrb_state *mrb, size_t nelem, size_t len)
{
  void *p;

  if (nelem > 0 && len > 0 &&
      nelem <= SIZE_MAX / len) {
    size_t size;
    size = nelem * len;
    p = mrb_realloc(mrb, 0, size);

    if (p) {
      memset(p, 0, size);
    }
  }
  else {
    p = NULL;
  }

  return p;
}

void
mrb_free(mrb_state *mrb, void *p)
{
  (mrb->allocf)(mrb, p, 0, mrb->ud);
}

#ifndef MRB_HEAP_PAGE_SIZE
#define MRB_HEAP_PAGE_SIZE 1024
#endif

struct heap_page {
  struct RBasic *freelist;
  struct heap_page *prev;
  struct heap_page *next;
  struct heap_page *free_next;
  struct heap_page *free_prev;
  mrb_bool old:1;
  RVALUE objects[MRB_HEAP_PAGE_SIZE];
};

static void
link_heap_page(mrb_state *mrb, struct heap_page *page)
{
  page->next = mrb->heaps;
  if (mrb->heaps)
    mrb->heaps->prev = page;
  mrb->heaps = page;
}

static void
unlink_heap_page(mrb_state *mrb, struct heap_page *page)
{
  if (page->prev)
    page->prev->next = page->next;
  if (page->next)
    page->next->prev = page->prev;
  if (mrb->heaps == page)
    mrb->heaps = page->next;
  page->prev = NULL;
  page->next = NULL;
}

static void
link_free_heap_page(mrb_state *mrb, struct heap_page *page)
{
  page->free_next = mrb->free_heaps;
  if (mrb->free_heaps) {
    mrb->free_heaps->free_prev = page;
  }
  mrb->free_heaps = page;
}

static void
unlink_free_heap_page(mrb_state *mrb, struct heap_page *page)
{
  if (page->free_prev)
    page->free_prev->free_next = page->free_next;
  if (page->free_next)
    page->free_next->free_prev = page->free_prev;
  if (mrb->free_heaps == page)
    mrb->free_heaps = page->free_next;
  page->free_prev = NULL;
  page->free_next = NULL;
}

static void
add_heap(mrb_state *mrb)
{
  struct heap_page *page = (struct heap_page *)mrb_calloc(mrb, 1, sizeof(struct heap_page));
  RVALUE *p, *e;
  struct RBasic *prev = NULL;

  for (p = page->objects, e=p+MRB_HEAP_PAGE_SIZE; p<e; p++) {
    p->as.free.tt = MRB_TT_FREE;
    p->as.free.next = prev;
    prev = &p->as.basic;
  }
  page->freelist = prev;

  link_heap_page(mrb, page);
  link_free_heap_page(mrb, page);
}

#define DEFAULT_GC_INTERVAL_RATIO 200
#define DEFAULT_GC_STEP_RATIO 200
#define DEFAULT_MAJOR_GC_INC_RATIO 200
#define is_generational(mrb) ((mrb)->is_generational_gc_mode)
#define is_major_gc(mrb) (is_generational(mrb) && (mrb)->gc_full)
#define is_minor_gc(mrb) (is_generational(mrb) && !(mrb)->gc_full)

void
mrb_init_heap(mrb_state *mrb)
{
  mrb->heaps = NULL;
  mrb->free_heaps = NULL;
  add_heap(mrb);
  mrb->gc_interval_ratio = DEFAULT_GC_INTERVAL_RATIO;
  mrb->gc_step_ratio = DEFAULT_GC_STEP_RATIO;
#ifndef MRB_GC_TURN_OFF_GENERATIONAL
  mrb->is_generational_gc_mode = TRUE;
  mrb->gc_full = TRUE;
#endif

#ifdef GC_PROFILE
  program_invoke_time = gettimeofday_time();
#endif
}

static void obj_free(mrb_state *mrb, struct RBasic *obj);

void
mrb_free_heap(mrb_state *mrb)
{
  struct heap_page *page = mrb->heaps;
  struct heap_page *tmp;
  RVALUE *p, *e;

  while (page) {
    tmp = page;
    page = page->next;
    for (p = tmp->objects, e=p+MRB_HEAP_PAGE_SIZE; p<e; p++) {
      if (p->as.free.tt != MRB_TT_FREE)
        obj_free(mrb, &p->as.basic);
    }
    mrb_free(mrb, tmp);
  }
}

static void
gc_protect(mrb_state *mrb, struct RBasic *p)
{
  if (mrb->arena_idx >= MRB_ARENA_SIZE) {
    /* arena overflow error */
    mrb->arena_idx = MRB_ARENA_SIZE - 4; /* force room in arena */
    mrb_raise(mrb, E_RUNTIME_ERROR, "arena overflow error");
  }
  mrb->arena[mrb->arena_idx++] = p;
}

void
mrb_gc_protect(mrb_state *mrb, mrb_value obj)
{
  if (mrb_special_const_p(obj)) return;
  gc_protect(mrb, mrb_basic_ptr(obj));
}

struct RBasic*
mrb_obj_alloc(mrb_state *mrb, enum mrb_vtype ttype, struct RClass *cls)
{
  struct RBasic *p;
  static const RVALUE RVALUE_zero = { { { MRB_TT_FALSE } } };

#ifdef MRB_GC_STRESS
  mrb_full_gc(mrb);
#endif
  if (mrb->gc_threshold < mrb->live) {
    mrb_incremental_gc(mrb);
  }
  if (mrb->free_heaps == NULL) {
    add_heap(mrb);
  }

  p = mrb->free_heaps->freelist;
  mrb->free_heaps->freelist = ((struct free_obj*)p)->next;
  if (mrb->free_heaps->freelist == NULL) {
    unlink_free_heap_page(mrb, mrb->free_heaps);
  }

  mrb->live++;
  gc_protect(mrb, p);
  *(RVALUE *)p = RVALUE_zero;
  p->tt = ttype;
  p->c = cls;
  paint_partial_white(mrb, p);
  return p;
}

static inline void
add_gray_list(mrb_state *mrb, struct RBasic *obj)
{
#ifdef MRB_GC_STRESS
  if (obj->tt > MRB_TT_MAXDEFINE) {
    abort();
  }
#endif
  paint_gray(obj);
  obj->gcnext = mrb->gray_list;
  mrb->gray_list = obj;
}

static void
mark_context_stack(mrb_state *mrb, struct mrb_context *c)
{
  size_t i;
  size_t e;

  e = c->stack - c->stbase;
  if (c->ci) e += c->ci->nregs;
  if (c->stbase + e > c->stend) e = c->stend - c->stbase;
  for (i=0; i<e; i++) {
    mrb_gc_mark_value(mrb, c->stbase[i]);
  }
}

static void
mark_context(mrb_state *mrb, struct mrb_context *c)
{
  size_t i;
  size_t e;
  mrb_callinfo *ci;

  /* mark stack */
  mark_context_stack(mrb, c);

  /* mark ensure stack */
  e = (c->ci) ? c->ci->eidx : 0;
  for (i=0; i<e; i++) {
    mrb_gc_mark(mrb, (struct RBasic*)c->ensure[i]);
  }
  /* mark closure */
  for (ci = c->cibase; ci <= c->ci; ci++) {
    if (!ci) continue;
    mrb_gc_mark(mrb, (struct RBasic*)ci->env);
    mrb_gc_mark(mrb, (struct RBasic*)ci->proc);
    mrb_gc_mark(mrb, (struct RBasic*)ci->target_class);
  }
  if (c->prev && c->prev->fib) {
    mrb_gc_mark(mrb, (struct RBasic*)c->prev->fib);
  }
}

static void
gc_mark_children(mrb_state *mrb, struct RBasic *obj)
{
  mrb_assert(is_gray(obj));
  paint_black(obj);
  mrb->gray_list = obj->gcnext;
  mrb_gc_mark(mrb, (struct RBasic*)obj->c);
  switch (obj->tt) {
  case MRB_TT_ICLASS:
    mrb_gc_mark(mrb, (struct RBasic*)((struct RClass*)obj)->super);
    break;

  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_SCLASS:
    {
      struct RClass *c = (struct RClass*)obj;

      mrb_gc_mark_mt(mrb, c);
      mrb_gc_mark(mrb, (struct RBasic*)c->super);
    }
    /* fall through */

  case MRB_TT_OBJECT:
  case MRB_TT_DATA:
    mrb_gc_mark_iv(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_PROC:
    {
      struct RProc *p = (struct RProc*)obj;

      mrb_gc_mark(mrb, (struct RBasic*)p->env);
      mrb_gc_mark(mrb, (struct RBasic*)p->target_class);
    }
    break;

  case MRB_TT_ENV:
    {
      struct REnv *e = (struct REnv*)obj;

      if (e->cioff < 0) {
        int i, len;

        len = (int)e->flags;
        for (i=0; i<len; i++) {
          mrb_gc_mark_value(mrb, e->stack[i]);
        }
      }
    }
    break;

  case MRB_TT_FIBER:
    {
      struct mrb_context *c = ((struct RFiber*)obj)->cxt;

      mark_context(mrb, c);
    }
    break;

  case MRB_TT_ARRAY:
    {
      struct RArray *a = (struct RArray*)obj;
      size_t i, e;

      for (i=0,e=a->len; i<e; i++) {
        mrb_gc_mark_value(mrb, a->ptr[i]);
      }
    }
    break;

  case MRB_TT_HASH:
    mrb_gc_mark_iv(mrb, (struct RObject*)obj);
    mrb_gc_mark_hash(mrb, (struct RHash*)obj);
    break;

  case MRB_TT_STRING:
    break;

  case MRB_TT_RANGE:
    {
      struct RRange *r = (struct RRange*)obj;

      if (r->edges) {
        mrb_gc_mark_value(mrb, r->edges->beg);
        mrb_gc_mark_value(mrb, r->edges->end);
      }
    }
    break;

  default:
    break;
  }
}

void
mrb_gc_mark(mrb_state *mrb, struct RBasic *obj)
{
  if (obj == 0) return;
  if (!is_white(obj)) return;
  mrb_assert((obj)->tt != MRB_TT_FREE);
  add_gray_list(mrb, obj);
}

static void
obj_free(mrb_state *mrb, struct RBasic *obj)
{
  DEBUG(printf("obj_free(%p,tt=%d)\n",obj,obj->tt));
  switch (obj->tt) {
    /* immediate - no mark */
  case MRB_TT_TRUE:
  case MRB_TT_FIXNUM:
  case MRB_TT_SYMBOL:
    /* cannot happen */
    return;

  case MRB_TT_FLOAT:
#ifdef MRB_WORD_BOXING
    break;
#else
    return;
#endif

  case MRB_TT_OBJECT:
    mrb_gc_free_iv(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_SCLASS:
    mrb_gc_free_mt(mrb, (struct RClass*)obj);
    mrb_gc_free_iv(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_ENV:
    {
      struct REnv *e = (struct REnv*)obj;

      if (e->cioff < 0) {
        mrb_free(mrb, e->stack);
        e->stack = NULL;
      }
    }
    break;

  case MRB_TT_FIBER:
    {
      struct mrb_context *c = ((struct RFiber*)obj)->cxt;

      mrb_free_context(mrb, c);
    }
    break;

  case MRB_TT_ARRAY:
    if (obj->flags & MRB_ARY_SHARED)
      mrb_ary_decref(mrb, ((struct RArray*)obj)->aux.shared);
    else
      mrb_free(mrb, ((struct RArray*)obj)->ptr);
    break;

  case MRB_TT_HASH:
    mrb_gc_free_iv(mrb, (struct RObject*)obj);
    mrb_gc_free_hash(mrb, (struct RHash*)obj);
    break;

  case MRB_TT_STRING:
    mrb_gc_free_str(mrb, (struct RString*)obj);
    break;

  case MRB_TT_RANGE:
    mrb_free(mrb, ((struct RRange*)obj)->edges);
    break;

  case MRB_TT_DATA:
    {
      struct RData *d = (struct RData*)obj;
      if (d->type && d->type->dfree) {
        d->type->dfree(mrb, d->data);
      }
      mrb_gc_free_iv(mrb, (struct RObject*)obj);
    }
    break;

  default:
    break;
  }
  obj->tt = MRB_TT_FREE;
}

static void
root_scan_phase(mrb_state *mrb)
{
  size_t i, e, j;

  if (!is_minor_gc(mrb)) {
    mrb->gray_list = NULL;
    mrb->atomic_gray_list = NULL;
  }

  mrb_gc_mark_gv(mrb);
  /* mark arena */
  for (i=0,e=mrb->arena_idx; i<e; i++) {
    mrb_gc_mark(mrb, mrb->arena[i]);
  }
  /* mark class hierarchy */
  mrb_gc_mark(mrb, (struct RBasic*)mrb->object_class);
  /* mark top_self */
  mrb_gc_mark(mrb, (struct RBasic*)mrb->top_self);
  /* mark exception */
  mrb_gc_mark(mrb, (struct RBasic*)mrb->exc);

  mark_context(mrb, mrb->root_c);
  if (mrb->root_c != mrb->c) {
    mark_context(mrb, mrb->c);
  }

  /* mark irep pool */
  if (mrb->irep) {
    size_t len = mrb->irep_len;
    if (len > mrb->irep_capa) len = mrb->irep_capa;
    for (i=0; i<len; i++) {
      mrb_irep *irep = mrb->irep[i];
      if (!irep) continue;
      for (j=0; j<irep->plen; j++) {
        mrb_gc_mark_value(mrb, irep->pool[j]);
      }
    }
  }
}

static size_t
gc_gray_mark(mrb_state *mrb, struct RBasic *obj)
{
  size_t children = 0;

  gc_mark_children(mrb, obj);

  switch (obj->tt) {
  case MRB_TT_ICLASS:
    children++;
    break;

  case MRB_TT_CLASS:
  case MRB_TT_SCLASS:
  case MRB_TT_MODULE:
    {
      struct RClass *c = (struct RClass*)obj;

      children += mrb_gc_mark_iv_size(mrb, (struct RObject*)obj);
      children += mrb_gc_mark_mt_size(mrb, c);
      children++;
    }
    break;

  case MRB_TT_OBJECT:
  case MRB_TT_DATA:
    children += mrb_gc_mark_iv_size(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_ENV:
    children += (int)obj->flags;
    break;

  case MRB_TT_FIBER:
    {
      struct mrb_context *c = ((struct RFiber*)obj)->cxt;
      size_t i;
      mrb_callinfo *ci;

      /* mark stack */
      i = c->stack - c->stbase;
      if (c->ci) i += c->ci->nregs;
      if (c->stbase + i > c->stend) i = c->stend - c->stbase;
      children += i;

      /* mark ensure stack */
      children += (c->ci) ? c->ci->eidx : 0;

      /* mark closure */
      if (c->cibase) {
        for (i=0, ci = c->cibase; ci <= c->ci; i++, ci++)
          ;
      }
      children += i;
    }
    break;

  case MRB_TT_ARRAY:
    {
      struct RArray *a = (struct RArray*)obj;
      children += a->len;
    }
    break;

  case MRB_TT_HASH:
    children += mrb_gc_mark_iv_size(mrb, (struct RObject*)obj);
    children += mrb_gc_mark_hash_size(mrb, (struct RHash*)obj);
    break;

  case MRB_TT_PROC:
  case MRB_TT_RANGE:
    children+=2;
    break;

  default:
    break;
  }
  return children;
}


static void
gc_mark_gray_list(mrb_state *mrb) {
  while (mrb->gray_list) {
    if (is_gray(mrb->gray_list))
      gc_mark_children(mrb, mrb->gray_list);
    else
      mrb->gray_list = mrb->gray_list->gcnext;
  }
}


static size_t
incremental_marking_phase(mrb_state *mrb, size_t limit)
{
  size_t tried_marks = 0;

  while (mrb->gray_list && tried_marks < limit) {
    tried_marks += gc_gray_mark(mrb, mrb->gray_list);
  }

  return tried_marks;
}

static void
final_marking_phase(mrb_state *mrb)
{
  mark_context_stack(mrb, mrb->root_c);
  gc_mark_gray_list(mrb);
  mrb_assert(mrb->gray_list == NULL);
  mrb->gray_list = mrb->atomic_gray_list;
  mrb->atomic_gray_list = NULL;
  gc_mark_gray_list(mrb);
  mrb_assert(mrb->gray_list == NULL);
}

static void
prepare_incremental_sweep(mrb_state *mrb)
{
  mrb->gc_state = GC_STATE_SWEEP;
  mrb->sweeps = mrb->heaps;
  mrb->gc_live_after_mark = mrb->live;
}

static size_t
incremental_sweep_phase(mrb_state *mrb, size_t limit)
{
  struct heap_page *page = mrb->sweeps;
  size_t tried_sweep = 0;

  while (page && (tried_sweep < limit)) {
    RVALUE *p = page->objects;
    RVALUE *e = p + MRB_HEAP_PAGE_SIZE;
    size_t freed = 0;
    int dead_slot = 1;
    int full = (page->freelist == NULL);

    if (is_minor_gc(mrb) && page->old) {
      /* skip a slot which doesn't contain any young object */
      p = e;
      dead_slot = 0;
    }
    while (p<e) {
      if (is_dead(mrb, &p->as.basic)) {
        if (p->as.basic.tt != MRB_TT_FREE) {
          obj_free(mrb, &p->as.basic);
          p->as.free.next = page->freelist;
          page->freelist = (struct RBasic*)p;
          freed++;
        }
      }
      else {
        if (!is_generational(mrb))
          paint_partial_white(mrb, &p->as.basic); /* next gc target */
        dead_slot = 0;
      }
      p++;
    }

    /* free dead slot */
    if (dead_slot && freed < MRB_HEAP_PAGE_SIZE) {
      struct heap_page *next = page->next;

      unlink_heap_page(mrb, page);
      unlink_free_heap_page(mrb, page);
      mrb_free(mrb, page);
      page = next;
    }
    else {
      if (full && freed > 0) {
        link_free_heap_page(mrb, page);
      }
      if (page->freelist == NULL && is_minor_gc(mrb))
        page->old = TRUE;
      else
        page->old = FALSE;
      page = page->next;
    }
    tried_sweep += MRB_HEAP_PAGE_SIZE;
    mrb->live -= freed;
    mrb->gc_live_after_mark -= freed;
  }
  mrb->sweeps = page;
  return tried_sweep;
}

static size_t
incremental_gc(mrb_state *mrb, size_t limit)
{
  switch (mrb->gc_state) {
  case GC_STATE_NONE:
    root_scan_phase(mrb);
    mrb->gc_state = GC_STATE_MARK;
    flip_white_part(mrb);
    return 0;
  case GC_STATE_MARK:
    if (mrb->gray_list) {
      return incremental_marking_phase(mrb, limit);
    }
    else {
      final_marking_phase(mrb);
      prepare_incremental_sweep(mrb);
      return 0;
    }
  case GC_STATE_SWEEP: {
     size_t tried_sweep = 0;
     tried_sweep = incremental_sweep_phase(mrb, limit);
     if (tried_sweep == 0)
       mrb->gc_state = GC_STATE_NONE;
     return tried_sweep;
  }
  default:
    /* unknown state */
    mrb_assert(0);
    return 0;
  }
}

static void
incremental_gc_until(mrb_state *mrb, enum gc_state to_state)
{
  do {
    incremental_gc(mrb, ~0);
  } while (mrb->gc_state != to_state);
}

static void
incremental_gc_step(mrb_state *mrb)
{
  size_t limit = 0, result = 0;
  limit = (GC_STEP_SIZE/100) * mrb->gc_step_ratio;
  while (result < limit) {
    result += incremental_gc(mrb, limit);
    if (mrb->gc_state == GC_STATE_NONE)
      break;
  }

  mrb->gc_threshold = mrb->live + GC_STEP_SIZE;
}

static void
clear_all_old(mrb_state *mrb)
{
  size_t origin_mode = mrb->is_generational_gc_mode;

  mrb_assert(is_generational(mrb));
  if (is_major_gc(mrb)) {
    /* finish the half baked GC */
    incremental_gc_until(mrb, GC_STATE_NONE);
  }

  /* Sweep the dead objects, then reset all the live objects
   * (including all the old objects, of course) to white. */
  mrb->is_generational_gc_mode = FALSE;
  prepare_incremental_sweep(mrb);
  incremental_gc_until(mrb, GC_STATE_NONE);
  mrb->is_generational_gc_mode = origin_mode;

  /* The gray objects has already been painted as white */
  mrb->atomic_gray_list = mrb->gray_list = NULL;
}

void
mrb_incremental_gc(mrb_state *mrb)
{
  if (mrb->gc_disabled) return;

  GC_INVOKE_TIME_REPORT("mrb_incremental_gc()");
  GC_TIME_START;

  if (is_minor_gc(mrb)) {
    incremental_gc_until(mrb, GC_STATE_NONE);
  }
  else {
    incremental_gc_step(mrb);
  }

  if (mrb->gc_state == GC_STATE_NONE) {
    mrb_assert(mrb->live >= mrb->gc_live_after_mark);
    mrb->gc_threshold = (mrb->gc_live_after_mark/100) * mrb->gc_interval_ratio;
    if (mrb->gc_threshold < GC_STEP_SIZE) {
      mrb->gc_threshold = GC_STEP_SIZE;
    }

    if (is_major_gc(mrb)) {
      mrb->majorgc_old_threshold = mrb->gc_live_after_mark/100 * DEFAULT_MAJOR_GC_INC_RATIO;
      mrb->gc_full = FALSE;
    }
    else if (is_minor_gc(mrb)) {
      if (mrb->live > mrb->majorgc_old_threshold) {
        clear_all_old(mrb);
        mrb->gc_full = TRUE;
      }
    }
  }

  GC_TIME_STOP_AND_REPORT;
}

/* Perform a full gc cycle */
void
mrb_full_gc(mrb_state *mrb)
{
  if (mrb->gc_disabled) return;
  GC_INVOKE_TIME_REPORT("mrb_full_gc()");
  GC_TIME_START;

  if (is_generational(mrb)) {
    /* clear all the old objects back to young */
    clear_all_old(mrb);
    mrb->gc_full = TRUE;
  }
  else if (mrb->gc_state != GC_STATE_NONE) {
    /* finish half baked GC cycle */
    incremental_gc_until(mrb, GC_STATE_NONE);
  }

  incremental_gc_until(mrb, GC_STATE_NONE);
  mrb->gc_threshold = (mrb->gc_live_after_mark/100) * mrb->gc_interval_ratio;

  if (is_generational(mrb)) {
    mrb->majorgc_old_threshold = mrb->gc_live_after_mark/100 * DEFAULT_MAJOR_GC_INC_RATIO;
    mrb->gc_full = FALSE;
  }

  GC_TIME_STOP_AND_REPORT;
}

void
mrb_garbage_collect(mrb_state *mrb)
{
  mrb_full_gc(mrb);
}

int
mrb_gc_arena_save(mrb_state *mrb)
{
  return mrb->arena_idx;
}

void
mrb_gc_arena_restore(mrb_state *mrb, int idx)
{
  mrb->arena_idx = idx;
}

/*
 * Field write barrier
 *   Paint obj(Black) -> value(White) to obj(Black) -> value(Gray).
 */

void
mrb_field_write_barrier(mrb_state *mrb, struct RBasic *obj, struct RBasic *value)
{
  if (!is_black(obj)) return;
  if (!is_white(value)) return;

  mrb_assert(!is_dead(mrb, value) && !is_dead(mrb, obj));
  mrb_assert(is_generational(mrb) || mrb->gc_state != GC_STATE_NONE);

  if (is_generational(mrb) || mrb->gc_state == GC_STATE_MARK) {
    add_gray_list(mrb, value);
  }
  else {
    mrb_assert(mrb->gc_state == GC_STATE_SWEEP);
    paint_partial_white(mrb, obj); /* for never write barriers */
  }
}

/*
 * Write barrier
 *   Paint obj(Black) to obj(Gray).
 *
 *   The object that is painted gray will be traversed atomically in final
 *   mark phase. So you use this write barrier if it's frequency written spot.
 *   e.g. Set element on Array.
 */

void
mrb_write_barrier(mrb_state *mrb, struct RBasic *obj)
{
  if (!is_black(obj)) return;

  mrb_assert(!is_dead(mrb, obj));
  mrb_assert(is_generational(mrb) || mrb->gc_state != GC_STATE_NONE);
  paint_gray(obj);
  obj->gcnext = mrb->atomic_gray_list;
  mrb->atomic_gray_list = obj;
}

/*
 *  call-seq:
 *     GC.start                     -> nil
 *
 *  Initiates full garbage collection.
 *
 */

static mrb_value
gc_start(mrb_state *mrb, mrb_value obj)
{
  mrb_full_gc(mrb);
  return mrb_nil_value();
}

/*
 *  call-seq:
 *     GC.enable    -> true or false
 *
 *  Enables garbage collection, returning <code>true</code> if garbage
 *  collection was previously disabled.
 *
 *     GC.disable   #=> false
 *     GC.enable    #=> true
 *     GC.enable    #=> false
 *
 */

static mrb_value
gc_enable(mrb_state *mrb, mrb_value obj)
{
  int old = mrb->gc_disabled;

  mrb->gc_disabled = FALSE;

  return mrb_bool_value(old);
}

/*
 *  call-seq:
 *     GC.disable    -> true or false
 *
 *  Disables garbage collection, returning <code>true</code> if garbage
 *  collection was already disabled.
 *
 *     GC.disable   #=> false
 *     GC.disable   #=> true
 *
 */

static mrb_value
gc_disable(mrb_state *mrb, mrb_value obj)
{
  int old = mrb->gc_disabled;

  mrb->gc_disabled = TRUE;

  return mrb_bool_value(old);
}

/*
 *  call-seq:
 *     GC.interval_ratio      -> fixnum
 *
 *  Returns ratio of GC interval. Default value is 200(%).
 *
 */

static mrb_value
gc_interval_ratio_get(mrb_state *mrb, mrb_value obj)
{
  return mrb_fixnum_value(mrb->gc_interval_ratio);
}

/*
 *  call-seq:
 *     GC.interval_ratio = fixnum    -> nil
 *
 *  Updates ratio of GC interval. Default value is 200(%).
 *  GC start as soon as after end all step of GC if you set 100(%).
 *
 */

static mrb_value
gc_interval_ratio_set(mrb_state *mrb, mrb_value obj)
{
  mrb_int ratio;

  mrb_get_args(mrb, "i", &ratio);
  mrb->gc_interval_ratio = ratio;
  return mrb_nil_value();
}

/*
 *  call-seq:
 *     GC.step_ratio    -> fixnum
 *
 *  Returns step span ratio of Incremental GC. Default value is 200(%).
 *
 */

static mrb_value
gc_step_ratio_get(mrb_state *mrb, mrb_value obj)
{
  return mrb_fixnum_value(mrb->gc_step_ratio);
}

/*
 *  call-seq:
 *     GC.step_ratio = fixnum   -> nil
 *
 *  Updates step span ratio of Incremental GC. Default value is 200(%).
 *  1 step of incrementalGC becomes long if a rate is big.
 *
 */

static mrb_value
gc_step_ratio_set(mrb_state *mrb, mrb_value obj)
{
  mrb_int ratio;

  mrb_get_args(mrb, "i", &ratio);
  mrb->gc_step_ratio = ratio;
  return mrb_nil_value();
}

static void
change_gen_gc_mode(mrb_state *mrb, mrb_int enable)
{
  if (is_generational(mrb) && !enable) {
    clear_all_old(mrb);
    mrb_assert(mrb->gc_state == GC_STATE_NONE);
    mrb->gc_full = FALSE;
  }
  else if (!is_generational(mrb) && enable) {
    incremental_gc_until(mrb, GC_STATE_NONE);
    mrb->majorgc_old_threshold = mrb->gc_live_after_mark/100 * DEFAULT_MAJOR_GC_INC_RATIO;
    mrb->gc_full = FALSE;
  }
  mrb->is_generational_gc_mode = enable;
}

/*
 *  call-seq:
 *     GC.generational_mode -> true or false
 *
 *  Returns generational or normal gc mode.
 *
 */

static mrb_value
gc_generational_mode_get(mrb_state *mrb, mrb_value self)
{
  return mrb_bool_value(mrb->is_generational_gc_mode);
}

/*
 *  call-seq:
 *     GC.generational_mode = true or false -> true or false
 *
 *  Changes to generational or normal gc mode.
 *
 */

static mrb_value
gc_generational_mode_set(mrb_state *mrb, mrb_value self)
{
  mrb_bool enable;

  mrb_get_args(mrb, "b", &enable);
  if (mrb->is_generational_gc_mode != enable)
    change_gen_gc_mode(mrb, enable);

  return mrb_bool_value(enable);
}

void
mrb_objspace_each_objects(mrb_state *mrb, each_object_callback* callback, void *data)
{
    struct heap_page* page = mrb->heaps;

    while (page != NULL) {
        RVALUE *p, *pend;

        p = page->objects;
        pend = p + MRB_HEAP_PAGE_SIZE;
        for (;p < pend; p++) {
           (*callback)(mrb, &p->as.basic, data);
        }

        page = page->next;
    }
}

#ifdef GC_TEST
#ifdef GC_DEBUG
static mrb_value gc_test(mrb_state *, mrb_value);
#endif
#endif

void
mrb_init_gc(mrb_state *mrb)
{
  struct RClass *gc;

  gc = mrb_define_module(mrb, "GC");

  mrb_define_class_method(mrb, gc, "start", gc_start, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gc, "enable", gc_enable, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gc, "disable", gc_disable, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gc, "interval_ratio", gc_interval_ratio_get, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gc, "interval_ratio=", gc_interval_ratio_set, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, gc, "step_ratio", gc_step_ratio_get, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gc, "step_ratio=", gc_step_ratio_set, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, gc, "generational_mode=", gc_generational_mode_set, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, gc, "generational_mode", gc_generational_mode_get, MRB_ARGS_NONE());
#ifdef GC_TEST
#ifdef GC_DEBUG
  mrb_define_class_method(mrb, gc, "test", gc_test, MRB_ARGS_NONE());
#endif
#endif
}

#ifdef GC_TEST
#ifdef GC_DEBUG
void
test_mrb_field_write_barrier(void)
{
  mrb_state *mrb = mrb_open();
  struct RBasic *obj, *value;

  puts("test_mrb_field_write_barrier");
  mrb->is_generational_gc_mode = FALSE;
  obj = mrb_basic_ptr(mrb_ary_new(mrb));
  value = mrb_basic_ptr(mrb_str_new_cstr(mrb, "value"));
  paint_black(obj);
  paint_partial_white(mrb,value);


  puts("  in GC_STATE_MARK");
  mrb->gc_state = GC_STATE_MARK;
  mrb_field_write_barrier(mrb, obj, value);

  mrb_assert(is_gray(value));


  puts("  in GC_STATE_SWEEP");
  paint_partial_white(mrb,value);
  mrb->gc_state = GC_STATE_SWEEP;
  mrb_field_write_barrier(mrb, obj, value);

  mrb_assert(obj->color & mrb->current_white_part);
  mrb_assert(value->color & mrb->current_white_part);


  puts("  fail with black");
  mrb->gc_state = GC_STATE_MARK;
  paint_white(obj);
  paint_partial_white(mrb,value);
  mrb_field_write_barrier(mrb, obj, value);

  mrb_assert(obj->color & mrb->current_white_part);


  puts("  fail with gray");
  mrb->gc_state = GC_STATE_MARK;
  paint_black(obj);
  paint_gray(value);
  mrb_field_write_barrier(mrb, obj, value);

  mrb_assert(is_gray(value));


  {
    puts("test_mrb_field_write_barrier_value");
    obj = mrb_basic_ptr(mrb_ary_new(mrb));
    mrb_value value = mrb_str_new_cstr(mrb, "value");
    paint_black(obj);
    paint_partial_white(mrb, mrb_basic_ptr(value));

    mrb->gc_state = GC_STATE_MARK;
    mrb_field_write_barrier_value(mrb, obj, value);

    mrb_assert(is_gray(mrb_basic_ptr(value)));
  }

  mrb_close(mrb);
}

void
test_mrb_write_barrier(void)
{
  mrb_state *mrb = mrb_open();
  struct RBasic *obj;

  puts("test_mrb_write_barrier");
  obj = mrb_basic_ptr(mrb_ary_new(mrb));
  paint_black(obj);

  puts("  in GC_STATE_MARK");
  mrb->gc_state = GC_STATE_MARK;
  mrb_write_barrier(mrb, obj);

  mrb_assert(is_gray(obj));
  mrb_assert(mrb->atomic_gray_list == obj);


  puts("  fail with gray");
  paint_gray(obj);
  mrb_write_barrier(mrb, obj);

  mrb_assert(is_gray(obj));

  mrb_close(mrb);
}

void
test_add_gray_list(void)
{
  mrb_state *mrb = mrb_open();
  struct RBasic *obj1, *obj2;

  puts("test_add_gray_list");
  change_gen_gc_mode(mrb, FALSE);
  mrb_assert(mrb->gray_list == NULL);
  obj1 = mrb_basic_ptr(mrb_str_new_cstr(mrb, "test"));
  add_gray_list(mrb, obj1);
  mrb_assert(mrb->gray_list == obj1);
  mrb_assert(is_gray(obj1));

  obj2 = mrb_basic_ptr(mrb_str_new_cstr(mrb, "test"));
  add_gray_list(mrb, obj2);
  mrb_assert(mrb->gray_list == obj2);
  mrb_assert(mrb->gray_list->gcnext == obj1);
  mrb_assert(is_gray(obj2));

  mrb_close(mrb);
}

void
test_gc_gray_mark(void)
{
  mrb_state *mrb = mrb_open();
  mrb_value obj_v, value_v;
  struct RBasic *obj;
  size_t gray_num = 0;

  puts("test_gc_gray_mark");

  puts("  in MRB_TT_CLASS");
  obj = (struct RBasic*)mrb->object_class;
  paint_gray(obj);
  gray_num = gc_gray_mark(mrb, obj);
  mrb_assert(is_black(obj));
  mrb_assert(gray_num > 1);

  puts("  in MRB_TT_ARRAY");
  obj_v = mrb_ary_new(mrb);
  value_v = mrb_str_new_cstr(mrb, "test");
  paint_gray(mrb_basic_ptr(obj_v));
  paint_partial_white(mrb, mrb_basic_ptr(value_v));
  mrb_ary_push(mrb, obj_v, value_v);
  gray_num = gc_gray_mark(mrb, mrb_basic_ptr(obj_v));
  mrb_assert(is_black(mrb_basic_ptr(obj_v)));
  mrb_assert(is_gray(mrb_basic_ptr(value_v)));
  mrb_assert(gray_num == 1);

  mrb_close(mrb);
}

void
test_incremental_gc(void)
{
  mrb_state *mrb = mrb_open();
  size_t max = ~0, live = 0, total = 0, freed = 0;
  RVALUE *free;
  struct heap_page *page;

  puts("test_incremental_gc");
  change_gen_gc_mode(mrb, FALSE);

  puts("  in mrb_full_gc");
  mrb_full_gc(mrb);

  mrb_assert(mrb->gc_state == GC_STATE_NONE);
  puts("  in GC_STATE_NONE");
  incremental_gc(mrb, max);
  mrb_assert(mrb->gc_state == GC_STATE_MARK);
  puts("  in GC_STATE_MARK");
  incremental_gc_until(mrb, GC_STATE_SWEEP);
  mrb_assert(mrb->gc_state == GC_STATE_SWEEP);

  puts("  in GC_STATE_SWEEP");
  page = mrb->heaps;
  while (page) {
    RVALUE *p = page->objects;
    RVALUE *e = p + MRB_HEAP_PAGE_SIZE;
    while (p<e) {
      if (is_black(&p->as.basic)) {
        live++;
      }
      if (is_gray(&p->as.basic) && !is_dead(mrb, &p->as.basic)) {
        printf("%p\n", &p->as.basic);
      }
      p++;
    }
    page = page->next;
    total += MRB_HEAP_PAGE_SIZE;
  }

  mrb_assert(mrb->gray_list == NULL);

  incremental_gc(mrb, max);
  mrb_assert(mrb->gc_state == GC_STATE_SWEEP);

  incremental_gc(mrb, max);
  mrb_assert(mrb->gc_state == GC_STATE_NONE);

  free = (RVALUE*)mrb->heaps->freelist;
  while (free) {
   freed++;
   free = (RVALUE*)free->as.free.next;
  }

  mrb_assert(mrb->live == live);
  mrb_assert(mrb->live == total-freed);

  puts("test_incremental_gc(gen)");
  incremental_gc_until(mrb, GC_STATE_SWEEP);
  change_gen_gc_mode(mrb, TRUE);

  mrb_assert(mrb->gc_full == FALSE);
  mrb_assert(mrb->gc_state == GC_STATE_NONE);

  puts("  in minor");
  mrb_assert(is_minor_gc(mrb));
  mrb_assert(mrb->majorgc_old_threshold > 0);
  mrb->majorgc_old_threshold = 0;
  mrb_incremental_gc(mrb);
  mrb_assert(mrb->gc_full == TRUE);
  mrb_assert(mrb->gc_state == GC_STATE_NONE);

  puts("  in major");
  mrb_assert(is_major_gc(mrb));
  do {
    mrb_incremental_gc(mrb);
  } while (mrb->gc_state != GC_STATE_NONE);
  mrb_assert(mrb->gc_full == FALSE);

  mrb_close(mrb);
}

void
test_incremental_sweep_phase(void)
{
  mrb_state *mrb = mrb_open();

  puts("test_incremental_sweep_phase");

  add_heap(mrb);
  mrb->sweeps = mrb->heaps;

  mrb_assert(mrb->heaps->next->next == NULL);
  mrb_assert(mrb->free_heaps->next->next == NULL);
  incremental_sweep_phase(mrb, MRB_HEAP_PAGE_SIZE*3);

  mrb_assert(mrb->heaps->next == NULL);
  mrb_assert(mrb->heaps == mrb->free_heaps);

  mrb_close(mrb);
}

static mrb_value
gc_test(mrb_state *mrb, mrb_value self)
{
  test_mrb_field_write_barrier();
  test_mrb_write_barrier();
  test_add_gray_list();
  test_gc_gray_mark();
  test_incremental_gc();
  test_incremental_sweep_phase();
  return mrb_nil_value();
}
#endif
#endif
