////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/V8Expression.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Executor.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Basics/json.h"
#include "Basics/json-utilities.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-shape-conv.h"

using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the v8 expression
////////////////////////////////////////////////////////////////////////////////

V8Expression::V8Expression(v8::Isolate* isolate, v8::Handle<v8::Function> func,
                           v8::Handle<v8::Object> constantValues, bool isSimple)
    : isolate(isolate),
      _func(),
      _constantValues(),
      _numExecutions(0),
      _isSimple(isSimple) {
  _func.Reset(isolate, func);
  _constantValues.Reset(isolate, constantValues);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the v8 expression
////////////////////////////////////////////////////////////////////////////////

V8Expression::~V8Expression() {
  _constantValues.Reset();
  _func.Reset();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue V8Expression::execute(v8::Isolate* isolate, Query* query,
                               arangodb::AqlTransaction* trx,
                               AqlItemBlock const* argv, size_t startPos,
                               std::vector<Variable const*> const& vars,
                               std::vector<RegisterId> const& regs) {
  size_t const n = vars.size();
  TRI_ASSERT(regs.size() == n);  // assert same vector length

  bool const hasRestrictions = !_attributeRestrictions.empty();

  v8::Handle<v8::Object> values = v8::Object::New(isolate);

  for (size_t i = 0; i < n; ++i) {
    auto reg = regs[i];

    auto const& value(argv->getValueReference(startPos, reg));

    if (value.isEmpty()) {
      continue;
    }

    auto document = argv->getDocumentCollection(reg);
    auto const& varname = vars[i]->name;

    if (hasRestrictions && value.isJson()) {
      // check if we can get away with constructing a partial JSON object
      auto it = _attributeRestrictions.find(vars[i]);

      if (it != _attributeRestrictions.end()) {
        // build a partial object
        values->ForceSet(
            TRI_V8_STD_STRING(varname),
            value.toV8Partial(isolate, trx, (*it).second, document));
        continue;
      }
    }

    // fallthrough to building the complete object

    // build the regular object
    values->ForceSet(TRI_V8_STD_STRING(varname),
                     value.toV8(isolate, trx, document));
  }

  TRI_ASSERT(query != nullptr);

  TRI_GET_GLOBALS();

  v8::Handle<v8::Value> result;

  auto old = v8g->_query;

  try {
    v8g->_query = static_cast<void*>(query);
    TRI_ASSERT(v8g->_query != nullptr);

    // set constant function arguments
    // note: constants are passed by reference so we can save re-creating them
    // on every invocation. this however means that these constants must not be
    // modified by the called function. there is a hash check in place below to
    // verify that constants don't get modified by the called function.
    // note: user-defined AQL functions are always called without constants
    // because they are opaque to the optimizer and the assumption that they
    // won't modify their arguments is unsafe
    auto constantValues = v8::Local<v8::Object>::New(isolate, _constantValues);

#ifdef TRI_ENABLE_FAILURE_TESTS
    // a hash function for hashing V8 object contents
    auto hasher =
        [](v8::Isolate* isolate, v8::Handle<v8::Value> const obj) -> uint64_t {
          std::unique_ptr<TRI_json_t> json(TRI_ObjectToJson(isolate, obj));

          if (json == nullptr) {
            return 0;
          }

          return TRI_FastHashJson(json.get());
        };

    // hash the constant values that we pass into V8
    uint64_t const hash = hasher(isolate, constantValues);
#endif

    v8::Handle<v8::Value> args[] = {
        values, constantValues,
        v8::Boolean::New(isolate, _numExecutions++ == 0)};

    // execute the function
    v8::TryCatch tryCatch;

    auto func = v8::Local<v8::Function>::New(isolate, _func);
    result = func->Call(func, 3, args);

#ifdef TRI_ENABLE_FAILURE_TESTS
    // now that the V8 function call is finished, check that our
    // constants actually were not modified
    uint64_t cmpHash = hasher(isolate, constantValues);

    if (hash != 0 && cmpHash != 0) {
      // only compare if both values are not 0
      TRI_ASSERT(hasher(isolate, constantValues) == hash);
    }
#endif

    v8g->_query = old;

    Executor::HandleV8Error(tryCatch, result);
  } catch (...) {
    v8g->_query = old;
    // bubble up exception
    throw;
  }

  // no exception was thrown if we get here
  std::unique_ptr<TRI_json_t> json;

  if (result->IsUndefined()) {
    // expression does not have any (defined) value. replace with null
    json.reset(TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
  } else {
    // expression had a result. convert it to JSON
    if (_isSimple) {
      json.reset(TRI_ObjectToJsonSimple(isolate, result));
    } else {
      json.reset(TRI_ObjectToJson(isolate, result));
    }
  }

  if (json.get() == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto j = new arangodb::basics::Json(TRI_UNKNOWN_MEM_ZONE, json.get());
  json.release();
  return AqlValue(j);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|//
// --SECTION--\\|/// @\\}"
// End
