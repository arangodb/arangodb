/*
** gc.h - garbage collector for mruby
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_GC_H
#define MRUBY_GC_H

#include "mruby.h"
#include "mruby/value.h"

typedef void (each_object_callback)(mrb_state *mrb, struct RBasic* obj, void *data);
void mrb_objspace_each_objects(mrb_state *mrb, each_object_callback* callback, void *data);
void mrb_free_context(mrb_state *mrb, struct mrb_context *c);

#endif  /* MRUBY_GC_H */
