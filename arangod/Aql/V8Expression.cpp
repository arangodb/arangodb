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
#include "Aql/Executor.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Basics/json.h"
#include "V8/v8-conv.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the v8 expression
////////////////////////////////////////////////////////////////////////////////

V8Expression::V8Expression (v8::Isolate* isolate,
                            v8::Persistent<v8::Function> func)
  : isolate(isolate),
    func(func) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the v8 expression
////////////////////////////////////////////////////////////////////////////////

V8Expression::~V8Expression () {
  func.Dispose(isolate);
  func.Clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue V8Expression::execute (Query* query,
                                triagens::arango::AqlTransaction* trx,
                                std::vector<TRI_document_collection_t const*>& docColls,
                                std::vector<AqlValue>& argv,
                                size_t startPos,
                                std::vector<Variable*> const& vars,
                                std::vector<RegisterId> const& regs) {
  size_t const n = vars.size();
  TRI_ASSERT(regs.size() == n); // assert same vector length

  v8::Handle<v8::Object> values = v8::Object::New();

  for (size_t i = 0; i < n; ++i) {
    auto varname = vars[i]->name;
    auto reg = regs[i];

    TRI_ASSERT(! argv[reg].isEmpty());

    values->Set(v8::String::New(varname.c_str(), (int) varname.size()), argv[startPos + reg].toV8(trx, docColls[reg]));
  }

  TRI_ASSERT(query != nullptr);

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  auto old = v8g->_query;
  v8g->_query = static_cast<void*>(query);
  TRI_ASSERT(v8g->_query != nullptr);

  // set function arguments
  v8::Handle<v8::Value> args[] = { values };

  // execute the function
  v8::TryCatch tryCatch;
  v8::Handle<v8::Value> result = func->Call(func, 1, args);

  v8g->_query = old;
    
  Executor::HandleV8Error(tryCatch, result);

  // no exception was thrown if we get here
  TRI_json_t* json = nullptr;

  if (result->IsUndefined()) {
    // expression does not have any (defined) value. replace with null
    json = TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE);
  }
  else {
    // expression had a result. convert it to JSON
    json = TRI_ObjectToJson(result);
  }

  if (json == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    auto j = new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, json);
    return AqlValue(j);
  }
  catch (...) {
    // prevent memleak
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    throw;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
