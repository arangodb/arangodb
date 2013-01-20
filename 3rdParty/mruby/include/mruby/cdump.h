/*
** mruby/cdump.h - mruby binary dumper (C source format)
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_CDUMP_H
#define MRUBY_CDUMP_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "mruby.h"
#ifdef DISABLE_STDIO
# error "Configuration conflict. Can't use with DISABLE_STDIO option."
#else
# include <stdio.h>
#endif

int mrb_cdump_irep(mrb_state *mrb, int n, FILE *f,const char *initname);

/* error code */
#define MRB_CDUMP_OK                     0
#define MRB_CDUMP_GENERAL_FAILURE        -1
#define MRB_CDUMP_WRITE_FAULT            -2
#define MRB_CDUMP_INVALID_IREP           -6
#define MRB_CDUMP_INVALID_ARGUMENT       -7

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* MRUBY_CDUMP_H */
