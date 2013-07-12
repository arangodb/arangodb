////////////////////////////////////////////////////////////////////////////////
/// @brief mruby utilities
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "mr-utils.h"

#include <regex.h>

#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"

#include "mruby/array.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/hash.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    ruby functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the time in seconds
////////////////////////////////////////////////////////////////////////////////

static mrb_value MR_Time (mrb_state* mrb, mrb_value self) {
  return mrb_float_value(TRI_microtime());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts json to ruby structure
////////////////////////////////////////////////////////////////////////////////

static mrb_value MR_JsonParse (mrb_state* mrb, mrb_value self) {
  char* errmsg;
  char* s;
  /* int res; */
  size_t l;
  TRI_json_t* json;

  /* res = */ mrb_get_args(mrb, "s", &s, &l);

  if (s == NULL) {
    return mrb_nil_value();
  }

  json = TRI_Json2String(TRI_UNKNOWN_MEM_ZONE, s, &errmsg);

  if (json == NULL) {
    mrb_value exc;

    exc = MR_ArangoError(mrb, TRI_ERROR_HTTP_CORRUPTED_JSON, errmsg);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, errmsg);

    mrb_exc_raise(mrb, exc);
    assert(false);
  }

  return MR_ObjectJson(mrb, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

mrb_value MR_ObjectJson (mrb_state* mrb, TRI_json_t const* json) {
  switch (json->_type) {
    case TRI_JSON_UNUSED:
      return mrb_nil_value();

    case TRI_JSON_NULL:
      return mrb_nil_value();

    case TRI_JSON_BOOLEAN:
      return json->_value._boolean ? mrb_true_value() : mrb_false_value();

    case TRI_JSON_NUMBER:
      return mrb_float_value(json->_value._number);

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE:
      // same for STRING and STRING_REFERENCE
      return mrb_str_new(mrb, json->_value._string.data, json->_value._string.length - 1);

    case TRI_JSON_ARRAY: {
      size_t n;
      size_t i;
      mrb_value a;
      TRI_json_t* sub;
      mrb_value key;
      mrb_value val;

      n = json->_value._objects._length;
      a = mrb_hash_new_capa(mrb, n);

      for (i = 0;  i < n;  i += 2) {
        sub = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);
 
        if (! TRI_IsStringJson(sub)) {
          continue;
        }

        key = mrb_str_new(mrb, sub->_value._string.data, sub->_value._string.length - 1);
        sub = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
        val = MR_ObjectJson(mrb, sub);

        mrb_hash_set(mrb, a, key, val);
      }

      return a;
    }

    case TRI_JSON_LIST: {
      size_t n;
      size_t i;
      mrb_value a;
      TRI_json_t* elm;
      mrb_value val;

      n = json->_value._objects._length;
      a = mrb_ary_new_capa(mrb, n);

      for (i = 0;  i < n;  ++i) {
        elm = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);
        val = MR_ObjectJson(mrb, elm);

        mrb_ary_set(mrb, a, i, val);
      }

      return a;
    }
  }

  return mrb_nil_value();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a new context
////////////////////////////////////////////////////////////////////////////////

mrb_state* MR_OpenShell () {
  mrb_state* mrb = mrb_open();

  mrb->ud = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(MR_state_t), true);

  return mrb;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes context
////////////////////////////////////////////////////////////////////////////////

void MR_CloseShell (mrb_state* mrb) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, mrb->ud);
  mrb_close(mrb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a ArangoError
////////////////////////////////////////////////////////////////////////////////

mrb_value MR_ArangoError (mrb_state* mrb, int errNum, char const* errMessage) {
  MR_state_t* mrs;
  mrb_value exc;
  mrb_value val;
  mrb_sym id;

  mrs = (MR_state_t*) mrb->ud;
  exc = mrb_exc_new(mrb, mrs->_arangoError, errMessage, strlen(errMessage));

  id = mrb_intern(mrb, "@error_num");
  val = mrb_fixnum_value(errNum);
  mrb_iv_set(mrb, exc, id, val);

  id = mrb_intern(mrb, "@error_message");
  val = mrb_str_new(mrb, errMessage, strlen(errMessage));
  mrb_iv_set(mrb, exc, id, val);

  return exc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an exception and stacktrace
////////////////////////////////////////////////////////////////////////////////

void TRI_LogRubyException (mrb_state* mrb, struct RObject* exc) {
  LOG_ERROR("cannot log ruby exception\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a file in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRubyFile (mrb_state* mrb, char const* filename) {
  bool ok;
  char* content;
  mrb_value result;

  content = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, filename, NULL);

  if (content == 0) {
    LOG_TRACE("cannot loaded ruby file '%s': %s", filename, TRI_last_error());
    return false;
  }

  ok = TRI_ExecuteRubyString(mrb, content, filename, false, &result);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, content);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all files from a directory in the current context
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRubyDirectory (mrb_state* mrb, char const* path) {
  TRI_vector_string_t files;
  bool result;
  regex_t re;
  size_t i;

  LOG_TRACE("loading ruby script directory: '%s'", path);

  files = TRI_FilesDirectory(path);

  regcomp(&re, "^(.*)\\.rb$", REG_ICASE | REG_EXTENDED);

  result = true;

  for (i = 0;  i < files._length;  ++i) {
    bool ok;
    char const* filename;
    char* full;

    filename = files._buffer[i];

    if (! regexec(&re, filename, 0, 0, 0) == 0) {
      continue;
    }

    full = TRI_Concatenate2File(path, filename);
    ok = TRI_ExecuteRubyFile(mrb, full);
    TRI_FreeString(TRI_CORE_MEM_ZONE, full);

    result = result && ok;

    if (! ok) {
      TRI_LogRubyException(mrb, mrb->exc);
    }
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within a mruby context, optionally print the result
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExecuteRubyString (mrb_state* mrb,
                            char const* script,
                            char const* name,
                            bool printResult,
                            mrb_value* result) {
  struct mrb_parser_state* parser;
  mrb_value r;
  int n;

  parser = mrb_parse_nstring(mrb, script, strlen(script), NULL);

  if (parser == 0 || parser->tree == 0 || 0 < parser->nerr) {
    LOG_DEBUG("failed to parse ruby script");

    if (parser != 0 && parser->pool != 0) {
      mrb_pool_close(parser->pool);
    }

    return false;
  }

  n = mrb_generate_code(mrb, parser);
  mrb_pool_close(parser->pool);

  if (n < 0) {
    LOG_DEBUG("failed to generate ruby code: %d", n);
    return false;
  }

  r = mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_top_self(mrb));

  if (mrb->exc) {
    if (printResult) {
      mrb_p(mrb, mrb_obj_value(mrb->exc));
    }

    mrb->exc = 0;
  }
  else if (printResult && ! mrb_nil_p(r)) {
    mrb_p(mrb, r);
  }

  if (result != NULL) {
    *result = r;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  module functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief init mruby utilities
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMRUtils (mrb_state* mrb) {
  MR_state_t* mrs;
  struct RClass *rcl;

  mrs = (MR_state_t*) mrb->ud;
  rcl = mrb->kernel_module;

  // .............................................................................
  // timing function
  // .............................................................................

  mrb_define_method(mrb, rcl, "time", MR_Time, ARGS_NONE());

  // .............................................................................
  // arango exception
  // .............................................................................

  mrs->_arangoError = mrb_define_class(mrb, "ArangoError", mrb->eStandardError_class);

  // .............................................................................
  // json parser and generator
  // .............................................................................

  rcl = mrb_define_class(mrb, "ArangoJson", mrb->object_class);

  mrb_define_class_method(mrb, rcl, "parse", MR_JsonParse, ARGS_REQ(1));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
