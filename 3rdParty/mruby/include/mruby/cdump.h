/*
** cdump.h - mruby binary dumper (C source format)
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include <stdio.h>

int mrb_cdump_irep(mrb_state *mrb, int n, FILE *f,const char *initname);

/* error code */
#define MRB_CDUMP_OK                     0
#define MRB_CDUMP_GENERAL_FAILURE        -1
