/*
** version.c - version information
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include "version.h"
#include <stdio.h>
#include "mruby/string.h"
#include "mruby/variable.h"

#define PRINT(type) puts(ruby_##type)
//#define MKSTR(type) mrb_obj_freeze(mrb_str_new(ruby_##type, sizeof(ruby_##type)-1))
#define MKSTR(type) mrb_str_new(mrb, ruby_##type, sizeof(ruby_##type)-1)

const char ruby_version[] = RUBY_VERSION;
const char ruby_release_date[] = RUBY_RELEASE_DATE;
const char ruby_platform[] = RUBY_PLATFORM;
const int ruby_patchlevel = RUBY_PATCHLEVEL;
const char ruby_engine[] = RUBY_ENGINE;

void
Init_version(mrb_state *mrb)
{
    char description[128];
    char copyright[128];
    mrb_value v = MKSTR(version);
    mrb_value d = MKSTR(release_date);
    mrb_value p = MKSTR(platform);
    mrb_value e = MKSTR(engine);
    mrb_value tmp;

    mrb_define_global_const(mrb, "RUBY_VERSION", v);
    mrb_define_global_const(mrb, "RUBY_RELEASE_DATE", d);
    mrb_define_global_const(mrb, "RUBY_PLATFORM", p);
    mrb_define_global_const(mrb, "RUBY_PATCHLEVEL", mrb_fixnum_value(RUBY_PATCHLEVEL));
    mrb_define_global_const(mrb, "RUBY_ENGINE", e);

    snprintf(description, sizeof(description), "ruby %s (%s %s %d) [%s]",
             RUBY_VERSION, RUBY_RELEASE_DATE, RUBY_RELEASE_STR,
             RUBY_RELEASE_NUM, RUBY_PLATFORM);
    //tmp = mrb_obj_freeze(mrb_str_new2(description));
    tmp = mrb_str_new2(mrb, description);
    mrb_define_global_const(mrb, "RUBY_DESCRIPTION", tmp);

    snprintf(copyright, sizeof(copyright), "ruby - Copyright (C) %d-%d %s",
             RUBY_BIRTH_YEAR, RUBY_RELEASE_YEAR, RUBY_AUTHOR);
    //tmp = mrb_obj_freeze(mrb_str_new2(copyright));
    tmp = mrb_str_new2(mrb, copyright);
    mrb_define_global_const(mrb, "RUBY_COPYRIGHT", tmp);

    /* obsolete constants */
    mrb_define_global_const(mrb, "VERSION", v);
    mrb_define_global_const(mrb, "RELEASE_DATE", d);
    mrb_define_global_const(mrb, "PLATFORM", p);
}

void
ruby_show_version(mrb_state *mrb)
{
  mrb_value v = mrb_const_get(mrb, mrb_obj_value(mrb->object_class), mrb_intern(mrb, "RUBY_DESCRIPTION"));

  if (mrb_type(v) != MRB_TT_STRING)
    return;

  puts(RSTRING_PTR(v));
  fflush(stdout);
}

void
ruby_show_copyright(mrb_state *mrb)
{
  mrb_value v = mrb_const_get(mrb, mrb_obj_value(mrb->object_class), mrb_intern(mrb, "RUBY_COPYRIGHT"));

  if (mrb_type(v) != MRB_TT_STRING)
    return;

  puts(RSTRING_PTR(v));
  exit(0);
}
