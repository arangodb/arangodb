/*
** state.c - mrb_state open/close functions
**
** See Copyright Notice in mruby.h
*/

#include <stdlib.h>
#include <string.h>
#include "mruby.h"
#include "mruby/class.h"
#include "mruby/irep.h"
#include "mruby/variable.h"

void mrb_init_heap(mrb_state*);
void mrb_init_core(mrb_state*);
void mrb_final_core(mrb_state*);

static mrb_value
inspect_main(mrb_state *mrb, mrb_value mod)
{
  return mrb_str_new(mrb, "main", 4);
}

mrb_state*
mrb_open_allocf(mrb_allocf f, void *ud)
{
  static const mrb_state mrb_state_zero = { 0 };
  static const struct mrb_context mrb_context_zero = { 0 };
  mrb_state *mrb;

#ifdef MRB_NAN_BOXING
  mrb_assert(sizeof(void*) == 4);
#endif

  mrb = (mrb_state *)(f)(NULL, NULL, sizeof(mrb_state), ud);
  if (mrb == NULL) return NULL;

  *mrb = mrb_state_zero;
  mrb->ud = ud;
  mrb->allocf = f;
  mrb->current_white_part = MRB_GC_WHITE_A;

  mrb_init_heap(mrb);
  mrb->c = (struct mrb_context*)mrb_malloc(mrb, sizeof(struct mrb_context));
  *mrb->c = mrb_context_zero;
  mrb->root_c = mrb->c;
  mrb_init_core(mrb);

  return mrb;
}

static void*
allocf(mrb_state *mrb, void *p, size_t size, void *ud)
{
  if (size == 0) {
    free(p);
    return NULL;
  }
  else {
    return realloc(p, size);
  }
}

struct alloca_header {
  struct alloca_header *next;
  char buf[];
};

void*
mrb_alloca(mrb_state *mrb, size_t size)
{
  struct alloca_header *p;

  p = (struct alloca_header*) mrb_malloc(mrb, sizeof(struct alloca_header)+size);
  if (p == NULL) return NULL;
  p->next = mrb->mems;
  mrb->mems = p;
  return (void*)p->buf;
}

static void
mrb_alloca_free(mrb_state *mrb)
{
  struct alloca_header *p;
  struct alloca_header *tmp;

  if (mrb == NULL) return;
  p = mrb->mems;

  while (p) {
    tmp = p;
    p = p->next;
    mrb_free(mrb, tmp);
  }
}

mrb_state*
mrb_open(void)
{
  mrb_state *mrb = mrb_open_allocf(allocf, NULL);

  return mrb;
}

void mrb_free_symtbl(mrb_state *mrb);
void mrb_free_heap(mrb_state *mrb);

void
mrb_irep_free(mrb_state *mrb, struct mrb_irep *irep)
{
  if (!(irep->flags & MRB_ISEQ_NO_FREE))
    mrb_free(mrb, irep->iseq);
  mrb_free(mrb, irep->pool);
  mrb_free(mrb, irep->syms);
  mrb_free(mrb, (void *)irep->filename);
  mrb_free(mrb, irep->lines);
  mrb_free(mrb, irep);
}

void
mrb_free_context(mrb_state *mrb, struct mrb_context *c)
{
  if (!c) return;
  mrb_free(mrb, c->stbase);
  mrb_free(mrb, c->cibase);
  mrb_free(mrb, c->rescue);
  mrb_free(mrb, c->ensure);
  mrb_free(mrb, c);
}

void
mrb_close(mrb_state *mrb)
{
  size_t i;

  mrb_final_core(mrb);

  /* free */
  mrb_gc_free_gv(mrb);
  for (i=0; i<mrb->irep_len; i++) {
    mrb_irep_free(mrb, mrb->irep[i]);
  }
  mrb_free(mrb, mrb->irep);
  mrb_free_context(mrb, mrb->root_c);
  mrb_free_symtbl(mrb);
  mrb_free_heap(mrb);
  mrb_alloca_free(mrb);
  mrb_free(mrb, mrb);
}

#ifndef MRB_IREP_ARRAY_INIT_SIZE
# define MRB_IREP_ARRAY_INIT_SIZE (256u)
#endif

mrb_irep*
mrb_add_irep(mrb_state *mrb)
{
  static const mrb_irep mrb_irep_zero = { 0 };
  mrb_irep *irep;

  if (!mrb->irep) {
    size_t max = MRB_IREP_ARRAY_INIT_SIZE;

    if (mrb->irep_len > max) max = mrb->irep_len+1;
    mrb->irep = (mrb_irep **)mrb_calloc(mrb, max, sizeof(mrb_irep*));
    mrb->irep_capa = max;
  }
  else if (mrb->irep_capa <= mrb->irep_len) {
    size_t i;
    size_t old_capa = mrb->irep_capa;
    while (mrb->irep_capa <= mrb->irep_len) {
      mrb->irep_capa *= 2;
    }
    mrb->irep = (mrb_irep **)mrb_realloc(mrb, mrb->irep, sizeof(mrb_irep*)*mrb->irep_capa);
    for (i = old_capa; i < mrb->irep_capa; i++) {
      mrb->irep[i] = NULL;
    }
  }
  irep = (mrb_irep *)mrb_malloc(mrb, sizeof(mrb_irep));
  *irep = mrb_irep_zero;
  mrb->irep[mrb->irep_len] = irep;
  irep->idx = mrb->irep_len++;

  return irep;
}

mrb_value
mrb_top_self(mrb_state *mrb)
{
  if (!mrb->top_self) {
    mrb->top_self = (struct RObject*)mrb_obj_alloc(mrb, MRB_TT_OBJECT, mrb->object_class);
    mrb_define_singleton_method(mrb, mrb->top_self, "inspect", inspect_main, MRB_ARGS_NONE());
    mrb_define_singleton_method(mrb, mrb->top_self, "to_s", inspect_main, MRB_ARGS_NONE());
  }
  return mrb_obj_value(mrb->top_self);
}
