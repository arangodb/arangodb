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

#include "V8Expression.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Aql/V8Executor.h"
#include "Aql/Variable.h"
#include "Basics/VelocyPackHelper.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief create the v8 expression
V8Expression::V8Expression(v8::Isolate* isolate, v8::Handle<v8::Function> func,
                           v8::Handle<v8::Object> constantValues, bool isSimple)
    : isolate(isolate),
      _func(),
      _state(),
      _constantValues(),
      _isSimple(isSimple) {
  _func.Reset(isolate, func);
  _state.Reset(isolate, v8::Object::New(isolate));
  _constantValues.Reset(isolate, constantValues);
}

/// @brief destroy the v8 expression
V8Expression::~V8Expression() {
  _constantValues.Reset();
  _state.Reset();
  _func.Reset();
}

/// @brief execute the expression
AqlValue V8Expression::execute(v8::Isolate* isolate, Query* query,
                               transaction::Methods* trx,
                               ExpressionContext* context,
                               bool& mustDestroy) {
  bool const hasRestrictions = !_attributeRestrictions.empty();

  v8::Handle<v8::Object> values = v8::Object::New(isolate);

  size_t const n = context->numRegisters();

  for (size_t i = 0; i < n; ++i) {
    AqlValue const& value = context->getRegisterValue(i);

    if (value.isEmpty()) {
      continue;
    }

    Variable const* var = context->getVariable(i);
    std::string const& varname = var->name;

    if (hasRestrictions && value.isObject()) {
      // check if we can get away with constructing a partial JSON object
      auto it = _attributeRestrictions.find(var);

      if (it != _attributeRestrictions.end()) {
        // build a partial object
        values->ForceSet(
            TRI_V8_STD_STRING(varname),
            value.toV8Partial(isolate, trx, (*it).second));
        continue;
      }
    }

    // fallthrough to building the complete object

    // build the regular object
    values->ForceSet(TRI_V8_STD_STRING(varname),
                     value.toV8(isolate, trx));
  }

  TRI_ASSERT(query != nullptr);

  TRI_GET_GLOBALS();

  v8::Handle<v8::Value> result;

  auto old = v8g->_query;

  try {
    v8g->_query = static_cast<void*>(query);
    TRI_ASSERT(v8g->_query != nullptr);
    
    auto state = v8::Local<v8::Object>::New(isolate, _state);

    // set constant function arguments
    // note: constants are passed by reference so we can save re-creating them
    // on every invocation. this however means that these constants must not be
    // modified by the called function. there is a hash check in place below to
    // verify that constants don't get modified by the called function.
    // note: user-defined AQL functions are always called without constants
    // because they are opaque to the optimizer and the assumption that they
    // won't modify their arguments is unsafe
    auto constantValues = v8::Local<v8::Object>::New(isolate, _constantValues);

    v8::Handle<v8::Value> args[] = { values, state, constantValues };

    // execute the function
    v8::TryCatch tryCatch;

    auto func = v8::Local<v8::Function>::New(isolate, _func);
    result = func->Call(func, 3, args);

    v8g->_query = old;

    V8Executor::HandleV8Error(tryCatch, result, nullptr, false);
  } catch (...) {
    v8g->_query = old;
    // bubble up exception
    throw;
  }

  // no exception was thrown if we get here

  if (result->IsUndefined()) {
    // expression does not have any (defined) value. replace with null
    mustDestroy = false;
    return AqlValue(basics::VelocyPackHelper::NullValue());
  } 
  
  // expression had a result. convert it to JSON
  if (_builder == nullptr) {
    _builder.reset(new VPackBuilder);
  } else {
    _builder->clear();
  }

  int res;
  if (_isSimple) {
    res = TRI_V8ToVPackSimple(isolate, *_builder.get(), result);
  } else {
    res = TRI_V8ToVPack(isolate, *_builder.get(), result, false);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  mustDestroy = true; // builder = dynamic data
  return AqlValue(_builder.get());
}

