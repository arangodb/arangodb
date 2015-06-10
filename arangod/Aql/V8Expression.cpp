////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, V8 expression
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/V8Expression.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Executor.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Basics/json.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the v8 expression
////////////////////////////////////////////////////////////////////////////////

V8Expression::V8Expression (v8::Isolate* isolate,
                            v8::Handle<v8::Function> func,
                            bool isSimple)
  : isolate(isolate),
    _func(),
    _isSimple(isSimple) {

  _func.Reset(isolate, func);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the v8 expression
////////////////////////////////////////////////////////////////////////////////

V8Expression::~V8Expression () {
  _func.Reset();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue V8Expression::execute (v8::Isolate* isolate,
                                Query* query,
                                triagens::arango::AqlTransaction* trx,
                                AqlItemBlock const* argv,
                                size_t startPos,
                                std::vector<Variable*> const& vars,
                                std::vector<RegisterId> const& regs) {
  size_t const n = vars.size();
  TRI_ASSERT_EXPENSIVE(regs.size() == n); // assert same vector length

  bool const hasRestrictions = ! _attributeRestrictions.empty();

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
        values->ForceSet(TRI_V8_STD_STRING(varname), value.toV8Partial(isolate, trx, (*it).second, document));
        continue;
      }
    }

    // fallthrough to building the complete object

    // build the regular object
    values->ForceSet(TRI_V8_STD_STRING(varname), value.toV8(isolate, trx, document));
  }

  TRI_ASSERT(query != nullptr);

  TRI_GET_GLOBALS();
    
  v8::Handle<v8::Value> result;

  auto old = v8g->_query;

  try {
    v8g->_query = static_cast<void*>(query);
    TRI_ASSERT(v8g->_query != nullptr);

    // set function arguments
    v8::Handle<v8::Value> args[] = { values };

    // execute the function
    v8::TryCatch tryCatch;

    auto func = v8::Local<v8::Function>::New(isolate, _func);
    result = func->Call(func, 1, args);

    v8g->_query = old;
  
    Executor::HandleV8Error(tryCatch, result);
  }
  catch (...) {
    v8g->_query = old;
    // bubble up exception
    throw;
  }

  // no exception was thrown if we get here
  std::unique_ptr<TRI_json_t> json;

  if (result->IsUndefined()) {
    // expression does not have any (defined) value. replace with null
    json.reset(TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
  }
  else {
    // expression had a result. convert it to JSON
    if (_isSimple) { 
      json.reset(TRI_ObjectToJsonSimple(isolate, result));
    }
    else {
      json.reset(TRI_ObjectToJson(isolate, result));
    }
  }

  if (json.get() == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  auto j = new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, json.get());
  json.release();
  return AqlValue(j);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End
