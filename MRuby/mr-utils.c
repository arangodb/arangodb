////////////////////////////////////////////////////////////////////////////////
/// @brief mruby utilities
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "mr-utils.h"

#include "BasicsC/json.h"

#include "mruby.h"
#include "mruby/array.h"
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
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

static mrb_value MR_ObjectJson (mrb_state* mrb, TRI_json_t const* json) {
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

        if (sub->_type != TRI_JSON_STRING) {
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
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
  int res;
  size_t l;
  TRI_json_t* json;

  res = mrb_get_args(mrb, "s", &s, &l);

  if (s == NULL) {
    return mrb_nil_value();
  }

  json = TRI_Json2String(TRI_UNKNOWN_MEM_ZONE, s, &errmsg);

  if (json == NULL) {
    return mrb_nil_value();
  }

  return MR_ObjectJson(mrb, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief init utilities
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMRUtils (mrb_state* mrb) {
  struct RClass *rcl;

  rcl = mrb->kernel_module;

  // .............................................................................
  // timing function
  // .............................................................................

  mrb_define_method(mrb, rcl, "time", MR_Time, ARGS_NONE());

  // .............................................................................
  // arango exception
  // .............................................................................

  rcl = mrb_define_class(mrb, "ArangoError", mrb->eStandardError_class);

  /*
mrb_value
mrb_obj_ivar_set(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_value val;
  mrb_sym id;

  mrb_get_args(mrb, "oo", &key, &val);
  id = mrb_to_id(mrb, key);
  mrb_iv_set(mrb, self, id, val);
  return val;
}
  */

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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
