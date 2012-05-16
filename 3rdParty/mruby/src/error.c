/*
** error.c - Exception class
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include "error.h"
#include "opcode.h"
#include "mruby/irep.h"
#include "mruby/proc.h"
#include "mruby/numeric.h"
#include "mruby/variable.h"
#include "mruby/string.h"
#include "mruby/class.h"

#define warn_printf printf

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

mrb_value
mrb_exc_new(mrb_state *mrb, struct RClass *c, const char *ptr, long len)
{
  return mrb_funcall(mrb, mrb_obj_value(c), "new", 1, mrb_str_new(mrb, ptr, len));
}

mrb_value
mrb_exc_new3(mrb_state *mrb, struct RClass* c, mrb_value str)
{
  //StringValue(str);
  mrb_string_value(mrb, &str);
  return mrb_funcall(mrb, mrb_obj_value(c), "new", 1, str);
}

//mrb_value make_exception(mrb_state *mrb, int argc, mrb_value *argv, int isstr);
/*
 * call-seq:
 *    Exception.new(msg = nil)   ->  exception
 *
 *  Construct a new Exception object, optionally passing in
 *  a message.
 */

static mrb_value
exc_initialize(mrb_state *mrb, mrb_value exc)
{
  mrb_value mesg;

  mrb_get_args(mrb, "o", &mesg);
  mrb_iv_set(mrb, exc, mrb_intern(mrb, "mesg"), mesg);

  return exc;
}

/*
 *  Document-method: exception
 *
 *  call-seq:
 *     exc.exception(string)  ->  an_exception or exc
 *
 *  With no argument, or if the argument is the same as the receiver,
 *  return the receiver. Otherwise, create a new
 *  exception object of the same class as the receiver, but with a
 *  message equal to <code>string.to_str</code>.
 *
 */

static mrb_value
exc_exception(mrb_state *mrb, mrb_value self)
{
  mrb_value exc;
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);

  if (argc == 0) return self;
  if (argc == 1 && mrb_obj_equal(mrb, self, argv[0])) return self;
  exc = mrb_obj_clone(mrb, self);
  exc_initialize(mrb, exc);

  return exc;
}

/*
 * call-seq:
 *   exception.to_s   ->  string
 *
 * Returns exception's message (or the name of the exception if
 * no message is set).
 */

static mrb_value
exc_to_s(mrb_state *mrb, mrb_value exc)
{
  mrb_value mesg = mrb_attr_get(mrb, exc, mrb_intern(mrb, "mesg"));

  if (mrb_nil_p(mesg)) return mrb_str_new2(mrb, mrb_obj_classname(mrb, exc));
  return mesg;
}

/*
 * call-seq:
 *   exception.message   ->  string
 *
 * Returns the result of invoking <code>exception.to_s</code>.
 * Normally this returns the exception's message or name. By
 * supplying a to_str method, exceptions are agreeing to
 * be used where Strings are expected.
 */

static mrb_value
exc_message(mrb_state *mrb, mrb_value exc)
{
  return mrb_funcall(mrb, exc, "to_s", 0);
}

/*
 * call-seq:
 *   exception.inspect   -> string
 *
 * Return this exception's class name an message
 */

static mrb_value
exc_inspect(mrb_state *mrb, mrb_value exc)
{
  mrb_value str, klass;

  klass = mrb_str_new2(mrb, mrb_obj_classname(mrb, exc));
  exc = mrb_obj_as_string(mrb, exc);
  if (RSTRING_LEN(exc) == 0) {
    return klass;
  }

  str = mrb_str_new2(mrb, "#<");
  mrb_str_append(mrb, str, klass);
  mrb_str_cat2(mrb, str, ": ");
  mrb_str_append(mrb, str, exc);
  mrb_str_cat2(mrb, str, ">");

  return str;
}


static mrb_value
exc_equal(mrb_state *mrb, mrb_value exc)
{
  mrb_value obj;
  mrb_value mesg;
  mrb_sym id_mesg = mrb_intern(mrb, "mesg");

  mrb_get_args(mrb, "o", &obj);

  if (mrb_obj_equal(mrb, exc, obj)) return mrb_true_value();

  if (mrb_obj_class(mrb, exc) != mrb_obj_class(mrb, obj)) {
    if ( mrb_respond_to(mrb, obj, mrb_intern(mrb, "message")) ) {
      mesg = mrb_funcall(mrb, obj, "message", 0);
    }
    else
      return mrb_false_value();
  }
  else {
    mesg = mrb_attr_get(mrb, obj, id_mesg);
  }

  if (!mrb_equal(mrb, mrb_attr_get(mrb, exc, id_mesg), mesg))
    return mrb_false_value();
  return mrb_true_value();
}

void
mrb_exc_raise(mrb_state *mrb, mrb_value exc)
{
    mrb->exc = mrb_object(exc);
    longjmp(*(jmp_buf*)mrb->jmp, 1);
}

void
mrb_raise_va(mrb_state *mrb, struct RClass *c, const char *fmt, va_list args)
{
  char buf[256];

  vsnprintf(buf, 256, fmt, args);
  mrb_exc_raise(mrb, mrb_exc_new(mrb, c, buf, strlen(buf)));
}

void
mrb_raise(mrb_state *mrb, struct RClass *c, const char *fmt, ...)
{
  va_list args;
  char buf[256];

  va_start(args, fmt);
  vsnprintf(buf, 256, fmt, args);
  va_end(args);
  mrb_exc_raise(mrb, mrb_exc_new(mrb, c, buf, strlen(buf)));
}

void
mrb_name_error(mrb_state *mrb, mrb_sym id, const char *fmt, ...)
{
  mrb_value exc, argv[2];
  va_list args;
  char buf[256];

  va_start(args, fmt);
  //argv[0] = mrb_vsprintf(fmt, args);
  vsnprintf(buf, 256, fmt, args);
  argv[0] = mrb_str_new(mrb, buf, strlen(buf));
  va_end(args);

  argv[1] = mrb_str_new_cstr(mrb, mrb_sym2name(mrb, id));
  exc = mrb_class_new_instance(mrb, 2, argv, E_NAME_ERROR);
  mrb_exc_raise(mrb, exc);
}
mrb_value
mrb_sprintf(mrb_state *mrb, const char *fmt, ...)
{
  va_list args;
  char buf[256];

  va_start(args, fmt);
  vsnprintf(buf, 256, fmt, args);
  va_end(args);
  return mrb_str_new(mrb, buf, strlen(buf));
}

void
mrb_warn(const char *fmt, ...)
{
  va_list args;
  char buf[256];

  va_start(args, fmt);
  snprintf(buf, 256, "warning: %s", fmt);
  printf(buf, args);
  va_end(args);
}


void
mrb_warning(const char *fmt, ...)
{
  va_list args;
  char buf[256];

  va_start(args, fmt);
  snprintf(buf, 256, "warning: %s", fmt);
  printf(buf, args);
  va_end(args);
}

void
mrb_bug(const char *fmt, ...)
{
  va_list args;
  char buf[256];

  va_start(args, fmt);
  snprintf(buf, 256, "bug: %s", fmt);
  printf(buf, args);
  va_end(args);
}

static const char *
mrb_strerrno(int err)
{
#define defined_error(name, num) if (err == num) return name;
#define undefined_error(name)
//#include "known_errors.inc"
#undef defined_error
#undef undefined_error
    return NULL;
}

void
mrb_bug_errno(const char *mesg, int errno_arg)
{
  if (errno_arg == 0)
    mrb_bug("%s: errno == 0 (NOERROR)", mesg);
  else {
    const char *errno_str = mrb_strerrno(errno_arg);
    if (errno_str)
      mrb_bug("%s: %s (%s)", mesg, strerror(errno_arg), errno_str);
    else
      mrb_bug("%s: %s (%d)", mesg, strerror(errno_arg), errno_arg);
  }
}

int
sysexit_status(mrb_state *mrb, mrb_value err)
{
  mrb_value st = mrb_iv_get(mrb, err, mrb_intern(mrb, "status"));
  return mrb_fixnum(st);
}

void
error_pos(void)
{
#if 0
  const char *sourcefile = mrb_sourcefile();
  int sourceline = mrb_sourceline();

  if (sourcefile) {
    if (sourceline == 0) {
      warn_printf("%s", sourcefile);
    }
    else if (mrb_frame_callee()) {
      warn_printf("%s:%d:in `%s'", sourcefile, sourceline,
                mrb_sym2name(mrb, mrb_frame_callee()));
    }
    else {
      warn_printf("%s:%d", sourcefile, sourceline);
    }
  }
#endif
}

static void
set_backtrace(mrb_state *mrb, mrb_value info, mrb_value bt)
{
        mrb_funcall(mrb, info, "set_backtrace", 1, bt);
}

mrb_value
make_exception(mrb_state *mrb, int argc, mrb_value *argv, int isstr)
{
  mrb_value mesg;
  int n;

  mesg = mrb_nil_value();
  switch (argc) {
    case 0:
    break;
    case 1:
      if (mrb_nil_p(argv[0]))
        break;
      if (isstr) {
        mesg = mrb_check_string_type(mrb, argv[0]);
        if (!mrb_nil_p(mesg)) {
          mesg = mrb_exc_new3(mrb, mrb->eRuntimeError_class, mesg);
          break;
        }
      }
      n = 0;
      goto exception_call;

    case 2:
    case 3:
      n = 1;
exception_call:
      //if (argv[0] == sysstack_error) return argv[0];

      //CONST_ID(mrb, exception, "exception");
      //mesg = mrb_check_funcall(mrb, argv[0], exception, n, argv+1);
      //if (mrb_nil_p(mesg)) {
      //  /* undef */
      //  mrb_raise(mrb, E_TYPE_ERROR, "exception class/object expected");
      //}
      if (mrb_respond_to(mrb, argv[0], mrb_intern(mrb, "exception"))) {
        mesg = mrb_funcall(mrb, argv[0], "exception", n, argv+1);
      }
      else {
        /* undef */
        mrb_raise(mrb, E_TYPE_ERROR, "exception class/object expected");
      }

      break;
    default:
      mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%d for 0..3)", argc);
      break;
  }
  if (argc > 0) {
    if (!mrb_obj_is_kind_of(mrb, mesg, mrb->eException_class))
      mrb_raise(mrb, E_TYPE_ERROR, "exception object expected");
    if (argc > 2)
        set_backtrace(mrb, mesg, argv[2]);
  }

  return mesg;
}

mrb_value
mrb_make_exception(mrb_state *mrb, int argc, mrb_value *argv)
{
  return make_exception(mrb, argc, argv, TRUE);
}

void
mrb_sys_fail(mrb_state *mrb, const char *mesg)
{
  mrb_raise(mrb, mrb->eRuntimeError_class, "%s", mesg);
}

static mrb_value
mrb_exc_c_exception(mrb_state *mrb, mrb_value exc)
{
  mrb_value *argv;
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_make_exception(mrb, argc, argv);
}

static mrb_value
mrb_exc_exception(mrb_state *mrb, mrb_value exc)
{
  mrb_value *argv;
  int argc;
  mrb_value exclass;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 0) return exc;
  exclass = mrb_obj_value(mrb_class(mrb, exc));
  return mrb_funcall(mrb, exclass, "exception", argc, argv);
}

void
mrb_init_exception(mrb_state *mrb)
{
  struct RClass *e;
  struct RClass *eTypeError_class;
  struct RClass *eArgumentError_class;
  struct RClass *eIndexError_class;
  struct RClass *eRangeError_class;
  struct RClass *eNameError_class;
  struct RClass *eNoMethodError_class;
  struct RClass *eScriptError_class;
  struct RClass *eSyntaxError_class;
  struct RClass *eLoadError_class;
  struct RClass *eSystemCallError_class;
  struct RClass *eLocalJumpError_class;
  struct RClass *eRegexpError_class;
  struct RClass *eZeroDivisionError_class;
  struct RClass *eEncodingError_class;
  struct RClass *eNotImpError_class;
  struct RClass *eFloatDomainError_class;
  struct RClass *eKeyError_class;

  mrb->eException_class = e = mrb_define_class(mrb, "Exception",           mrb->object_class);         /* 15.2.22 */
  mrb_define_class_method(mrb, e, "exception", mrb_instance_new, ARGS_ANY());
  mrb_define_method(mrb, e, "exception", exc_exception, ARGS_ANY());
  mrb_define_method(mrb, e, "initialize", exc_initialize, ARGS_ANY());
  mrb_define_method(mrb, e, "==", exc_equal, ARGS_REQ(1));
  mrb_define_method(mrb, e, "to_s", exc_to_s, ARGS_NONE());
  mrb_define_method(mrb, e, "message", exc_message, ARGS_NONE());
  mrb_define_method(mrb, e, "inspect", exc_inspect, ARGS_NONE());

  mrb->eStandardError_class     = mrb_define_class(mrb, "StandardError",       mrb->eException_class);     /* 15.2.23 */
  mrb->eRuntimeError_class      = mrb_define_class(mrb, "RuntimeError",        mrb->eStandardError_class); /* 15.2.28 */

  eTypeError_class              = mrb_define_class(mrb, "TypeError",           mrb->eStandardError_class); /* 15.2.29 */
  eArgumentError_class          = mrb_define_class(mrb, "ArgumentError",       mrb->eStandardError_class); /* 15.2.24 */
  eIndexError_class             = mrb_define_class(mrb, "IndexError",          mrb->eStandardError_class); /* 15.2.33 */
  eRangeError_class             = mrb_define_class(mrb, "RangeError",          mrb->eStandardError_class); /* 15.2.26 */
  eNameError_class              = mrb_define_class(mrb, "NameError",           mrb->eStandardError_class); /* 15.2.31 */

  eNoMethodError_class          = mrb_define_class(mrb, "NoMethodError",       eNameError_class);          /* 15.2.32 */
  eScriptError_class            = mrb_define_class(mrb, "ScriptError",         mrb->eException_class);     /* 15.2.37 */
  eSyntaxError_class            = mrb_define_class(mrb, "SyntaxError",         eScriptError_class);        /* 15.2.38 */
  eLoadError_class              = mrb_define_class(mrb, "LoadError",           eScriptError_class);        /* 15.2.39 */
  eSystemCallError_class        = mrb_define_class(mrb, "SystemCallError",     mrb->eStandardError_class); /* 15.2.36 */
  eLocalJumpError_class         = mrb_define_class(mrb, "LocalJumpError",      mrb->eStandardError_class); /* 15.2.25 */
  eRegexpError_class            = mrb_define_class(mrb, "RegexpError",         mrb->eStandardError_class); /* 15.2.27 */
  eZeroDivisionError_class      = mrb_define_class(mrb, "ZeroDivisionError",   mrb->eStandardError_class); /* 15.2.30 */

  eEncodingError_class          = mrb_define_class(mrb, "EncodingError",       mrb->eStandardError_class);
  eNotImpError_class            = mrb_define_class(mrb, "NotImplementedError", eScriptError_class);

  eFloatDomainError_class       = mrb_define_class(mrb, "FloatDomainError",    eRangeError_class);
  eKeyError_class               = mrb_define_class(mrb, "KeyError",            eIndexError_class);
}
