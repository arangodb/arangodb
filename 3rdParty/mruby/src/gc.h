/*
** gc.h - garbage collector for RiteVM
** 
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_GC_H
#define MRUBY_GC_H

typedef struct {
  union {
    struct free_obj {
      MRUBY_OBJECT_HEADER;
      struct RBasic *next;
    } free;
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

#endif  /* MRUBY_GC_H */
