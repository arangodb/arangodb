/*
** error.c - Exception class
**
** See Copyright Notice in mruby.h
*/

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/irep.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "error.h"

mrb_value
mrb_exc_new(mrb_state *mrb, struct RClass *c, const char *ptr, long len)
{
  return mrb_funcall(mrb, mrb_obj_value(c), "new", 1, mrb_str_new(mrb, ptr, len));
}

mrb_value
mrb_exc_new3(mrb_state *mrb, struct RClass* c, mrb_value str)
{
  str = mrb_str_to_str(mrb, str);
  return mrb_funcall(mrb, mrb_obj_value(c), "new", 1, str);
}

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

  if (mrb_get_args(mrb, "|o", &mesg) == 1) {
    mrb_iv_set(mrb, exc, mrb_intern2(mrb, "mesg", 4), mesg);
  }
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
  mrb_value a;
  int argc;

  argc = mrb_get_args(mrb, "|o", &a);
  if (argc == 0) return self;
  if (mrb_obj_equal(mrb, self, a)) return self;
  exc = mrb_obj_clone(mrb, self);
  mrb_iv_set(mrb, exc, mrb_intern2(mrb, "mesg", 4), a);

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
  mrb_value mesg = mrb_attr_get(mrb, exc, mrb_intern2(mrb, "mesg", 4));

  if (mrb_nil_p(mesg)) return mrb_str_new_cstr(mrb, mrb_obj_classname(mrb, exc));
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
  mrb_value str, mesg, file, line;

  mesg = mrb_attr_get(mrb, exc, mrb_intern2(mrb, "mesg", 4));
  file = mrb_attr_get(mrb, exc, mrb_intern2(mrb, "file", 4));
  line = mrb_attr_get(mrb, exc, mrb_intern2(mrb, "line", 4));

  if (!mrb_nil_p(file) && !mrb_nil_p(line)) {
    str = file;
    mrb_str_cat(mrb, str, ":", 1);
    mrb_str_append(mrb, str, line);
    mrb_str_cat(mrb, str, ": ", 2);
    if (!mrb_nil_p(mesg) && RSTRING_LEN(mesg) > 0) {
      mrb_str_append(mrb, str, mesg);
      mrb_str_cat(mrb, str, " (", 2);
    }
    mrb_str_cat_cstr(mrb, str, mrb_obj_classname(mrb, exc));
    if (!mrb_nil_p(mesg) && RSTRING_LEN(mesg) > 0) {
      mrb_str_cat(mrb, str, ")", 1);
    }
  }
  else {
    str = mrb_str_new_cstr(mrb, mrb_obj_classname(mrb, exc));
    if (!mrb_nil_p(mesg) && RSTRING_LEN(mesg) > 0) {
      mrb_str_cat(mrb, str, ": ", 2);
      mrb_str_append(mrb, str, mesg);
    }
    else {
      mrb_str_cat(mrb, str, ": ", 2);
      mrb_str_cat_cstr(mrb, str, mrb_obj_classname(mrb, exc));
    }
  }
  return str;
}


static mrb_value
exc_equal(mrb_state *mrb, mrb_value exc)
{
  mrb_value obj;
  mrb_value mesg;
  mrb_bool equal_p;
  mrb_sym id_mesg = mrb_intern2(mrb, "mesg", 4);

  mrb_get_args(mrb, "o", &obj);
  if (mrb_obj_equal(mrb, exc, obj)) {
    equal_p = 1;
  }
  else {
    if (mrb_obj_class(mrb, exc) != mrb_obj_class(mrb, obj)) {
      if (mrb_respond_to(mrb, obj, mrb_intern2(mrb, "message", 7))) {
        mesg = mrb_funcall(mrb, obj, "message", 0);
      }
      else
        return mrb_false_value();
    }
    else {
      mesg = mrb_attr_get(mrb, obj, id_mesg);
    }

    equal_p = mrb_equal(mrb, mrb_attr_get(mrb, exc, id_mesg), mesg);
  }

  return mrb_bool_value(equal_p);
}

static void
exc_debug_info(mrb_state *mrb, struct RObject *exc)
{
  mrb_callinfo *ci = mrb->c->ci;
  mrb_code *pc = ci->pc;

  mrb_obj_iv_set(mrb, exc, mrb_intern2(mrb, "ciidx", 5), mrb_fixnum_value(ci - mrb->c->cibase));
  ci--;
  while (ci >= mrb->c->cibase) {
    if (ci->proc && !MRB_PROC_CFUNC_P(ci->proc)) {
      mrb_irep *irep = ci->proc->body.irep;

      if (irep->filename && irep->lines && irep->iseq <= pc && pc < irep->iseq + irep->ilen) {
        mrb_obj_iv_set(mrb, exc, mrb_intern2(mrb, "file", 4), mrb_str_new_cstr(mrb, irep->filename));
        mrb_obj_iv_set(mrb, exc, mrb_intern2(mrb, "line", 4), mrb_fixnum_value(irep->lines[pc - irep->iseq - 1]));
        return;
      }
    }
    pc = ci->pc;
    ci--;
  }
}

void
mrb_exc_raise(mrb_state *mrb, mrb_value exc)
{
  mrb->exc = mrb_obj_ptr(exc);
  exc_debug_info(mrb, mrb->exc);
  if (!mrb->jmp) {
    mrb_p(mrb, exc);
    abort();
  }
  mrb_longjmp(mrb);
}

void
mrb_raise(mrb_state *mrb, struct RClass *c, const char *msg)
{
  mrb_value mesg;
  mesg = mrb_str_new_cstr(mrb, msg);
  mrb_exc_raise(mrb, mrb_exc_new3(mrb, c, mesg));
}

mrb_value
mrb_vformat(mrb_state *mrb, const char *format, va_list ap)
{
  const char *p = format;
  const char *b = p;
  ptrdiff_t size;
  mrb_value ary = mrb_ary_new_capa(mrb, 4);

  while (*p) {
    const char c = *p++;

    if (c == '%') {
      if (*p == 'S') {
        size = p - b - 1;
        mrb_ary_push(mrb, ary, mrb_str_new(mrb, b, size));
        mrb_ary_push(mrb, ary, va_arg(ap, mrb_value));
        b = p + 1;
      }
    }
    else if (c == '\\') {
      if (*p) {
        size = p - b - 1;
        mrb_ary_push(mrb, ary, mrb_str_new(mrb, b, size));
        mrb_ary_push(mrb, ary, mrb_str_new(mrb, p, 1));
        b = ++p;
      }
      else {
        break;
      }
    }
  }
  if (b == format) {
    return mrb_str_new_cstr(mrb, format);
  }
  else {
    size = p - b;
    mrb_ary_push(mrb, ary, mrb_str_new(mrb, b, size));
    return mrb_ary_join(mrb, ary, mrb_str_new(mrb,NULL,0));
  }
}

mrb_value
mrb_format(mrb_state *mrb, const char *format, ...)
{
  va_list ap;
  mrb_value str;

  va_start(ap, format);
  str = mrb_vformat(mrb, format, ap);
  va_end(ap);

  return str;
}

void
mrb_raisef(mrb_state *mrb, struct RClass *c, const char *fmt, ...)
{
  va_list args;
  mrb_value mesg;

  va_start(args, fmt);
  mesg = mrb_vformat(mrb, fmt, args);
  va_end(args);
  mrb_exc_raise(mrb, mrb_exc_new3(mrb, c, mesg));
}

void
mrb_name_error(mrb_state *mrb, mrb_sym id, const char *fmt, ...)
{
  mrb_value exc;
  mrb_value argv[2];
  va_list args;

  va_start(args, fmt);
  argv[0] = mrb_vformat(mrb, fmt, args);
  va_end(args);

  argv[1] = mrb_symbol_value(id);
  exc = mrb_obj_new(mrb, E_NAME_ERROR, 2, argv);
  mrb_exc_raise(mrb, exc);
}

void
mrb_warn(mrb_state *mrb, const char *fmt, ...)
{
#ifdef ENABLE_STDIO
  va_list ap;
  mrb_value str;

  va_start(ap, fmt);
  str = mrb_vformat(mrb, fmt, ap);
  fputs("warning: ", stderr);
  fwrite(RSTRING_PTR(str), RSTRING_LEN(str), 1, stderr);
  va_end(ap);
#endif
}

void
mrb_bug(mrb_state *mrb, const char *fmt, ...)
{
#ifdef ENABLE_STDIO
  va_list ap;
  mrb_value str;

  va_start(ap, fmt);
  str = mrb_vformat(mrb, fmt, ap);
  fputs("bug: ", stderr);
  fwrite(RSTRING_PTR(str), RSTRING_LEN(str), 1, stderr);
  va_end(ap);
#endif
  exit(EXIT_FAILURE);
}

int
sysexit_status(mrb_state *mrb, mrb_value err)
{
  mrb_value st = mrb_iv_get(mrb, err, mrb_intern2(mrb, "status", 6));
  return mrb_fixnum(st);
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
          mesg = mrb_exc_new3(mrb, E_RUNTIME_ERROR, mesg);
          break;
        }
      }
      n = 0;
      goto exception_call;

    case 2:
    case 3:
      n = 1;
exception_call:
      {
        mrb_sym exc = mrb_intern2(mrb, "exception", 9);
        if (mrb_respond_to(mrb, argv[0], exc)) {
          mesg = mrb_funcall_argv(mrb, argv[0], exc, n, argv+1);
        }
        else {
          /* undef */
          mrb_raise(mrb, E_TYPE_ERROR, "exception class/object expected");
        }
      }

      break;
    default:
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 0..3)", mrb_fixnum_value(argc));
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
  struct RClass *sce;
  mrb_int no;

  no = (mrb_int)errno;
  if (mrb_class_defined(mrb, "SystemCallError")) {
    sce = mrb_class_get(mrb, "SystemCallError");
    if (mesg != NULL) {
      mrb_funcall(mrb, mrb_obj_value(sce), "_sys_fail", 2, mrb_fixnum_value(no), mrb_str_new_cstr(mrb, mesg));
    }
    else {
      mrb_funcall(mrb, mrb_obj_value(sce), "_sys_fail", 1, mrb_fixnum_value(no));
    }
  }
  else {
    mrb_raise(mrb, E_RUNTIME_ERROR, mesg);
  }
}

void
mrb_init_exception(mrb_state *mrb)
{
  struct RClass *e;

  mrb->eException_class = e = mrb_define_class(mrb, "Exception",           mrb->object_class);         /* 15.2.22 */
  mrb_define_class_method(mrb, e, "exception", mrb_instance_new, MRB_ARGS_ANY());
  mrb_define_method(mrb, e, "exception", exc_exception, MRB_ARGS_ANY());
  mrb_define_method(mrb, e, "initialize", exc_initialize, MRB_ARGS_ANY());
  mrb_define_method(mrb, e, "==", exc_equal, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, e, "to_s", exc_to_s, MRB_ARGS_NONE());
  mrb_define_method(mrb, e, "message", exc_message, MRB_ARGS_NONE());
  mrb_define_method(mrb, e, "inspect", exc_inspect, MRB_ARGS_NONE());

  mrb->eStandardError_class     = mrb_define_class(mrb, "StandardError",       mrb->eException_class); /* 15.2.23 */
  mrb_define_class(mrb, "RuntimeError", mrb->eStandardError_class);                                    /* 15.2.28 */
  e = mrb_define_class(mrb, "ScriptError",  mrb->eException_class);                                    /* 15.2.37 */
  mrb_define_class(mrb, "SyntaxError",  e);                                                            /* 15.2.38 */
}
