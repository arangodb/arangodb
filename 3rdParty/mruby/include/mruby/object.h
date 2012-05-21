/*
** object.h - Object, NilClass, TrueClass, FalseClass class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_OBJECT_H
#define MRUBY_OBJECT_H

#define MRUBY_OBJECT_HEADER \
  enum mrb_vtype tt:8; \
  int color:3;\
  unsigned int flags:21;\
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
  MRUBY_OBJECT_HEADER;
};

struct RObject {
  MRUBY_OBJECT_HEADER;
  struct kh_iv *iv;
};

#define mrb_obj_ptr(v)    ((struct RObject*)((v).value.p))
#define RBASIC(obj)  ((struct RBasic*)((obj).value.p))
#define RBASIC_KLASS(v) ((struct RClass *)(((struct RBasic*)((v).value.p))->c))
#define ROBJECT(v) ((struct RObject*)((v).value.p))
#define ROBJECT_IVPTR(v) (((struct RObject*)((v).value.p))->iv)
#define ROBJECT_NUMIV(v) (ROBJECT_IVPTR(v) ? ROBJECT_IVPTR(v)->size : 0)
#endif  /* MRUBY_OBJECT_H */
