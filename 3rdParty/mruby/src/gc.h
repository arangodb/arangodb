/*
** gc.h - garbage collector for RiteVM
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_GC_H
#define MRUBY_GC_H

#if defined(__cplusplus)
extern "C" {
#endif

struct free_obj {
  MRUBY_OBJECT_HEADER;
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
    struct RStruct structdata;
    struct RProc procdata;
#ifdef INCLUDE_REGEXP
    struct RMatch match;
    struct RRegexp regexp;
#endif
  } as;
} RVALUE;

void mrb_gc_mark_gv(mrb_state*);
void mrb_gc_free_gv(mrb_state*);
void mrb_gc_mark_iv(mrb_state*, struct RObject*);
size_t mrb_gc_mark_iv_size(mrb_state*, struct RObject*);
void mrb_gc_free_iv(mrb_state*, struct RObject*);
void mrb_gc_mark_mt(mrb_state*, struct RClass*);
size_t mrb_gc_mark_mt_size(mrb_state*, struct RClass*);
void mrb_gc_free_mt(mrb_state*, struct RClass*);
void mrb_gc_mark_ht(mrb_state*, struct RHash*);
size_t mrb_gc_mark_ht_size(mrb_state*, struct RHash*);
void mrb_gc_free_ht(mrb_state*, struct RHash*);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* MRUBY_GC_H */
