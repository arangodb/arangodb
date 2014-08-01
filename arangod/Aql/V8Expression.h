////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, V8 execution context
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

#ifndef ARANGODB_AQL_V8_EXPRESSION_H
#define ARANGODB_AQL_V8_EXPRESSION_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "BasicsC/json.h"
#include "Aql/Types.h"
#include "V8/v8-conv.h"
#include <v8.h>

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                               struct V8Expression
// -----------------------------------------------------------------------------

    struct V8Expression {

////////////////////////////////////////////////////////////////////////////////
/// @brief create the v8 expression
////////////////////////////////////////////////////////////////////////////////

      V8Expression (v8::Isolate* isolate,
                    v8::Persistent<v8::Function> func) 
        : isolate(isolate),
          func(func) {
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the v8 expression
////////////////////////////////////////////////////////////////////////////////

      ~V8Expression () {
        func.Dispose(isolate);
        func.Clear();
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

      AqlValue* execute (AqlValue const** argv, 
                         std::vector<Variable*> vars,
                         std::vector<RegisterId> regs) {
        // TODO: decide whether a separate handle scope is needed

        v8::Handle<v8::Object> values = v8::Object::New();
        for (size_t i = 0; i < vars.size(); ++i) {
          auto varname = vars[i]->name;
          auto reg = regs[i];

          values->Set(v8::String::New(varname.c_str(), (int) varname.size()), argv[reg]->toV8());
        }

        v8::Handle<v8::Value> args[] = { values };
        v8::Handle<v8::Value> result = func->Call(func, 1, args);

        TRI_json_t* json = TRI_ObjectToJson(result);

        if (json == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }

        return new AqlValue(new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, json));
      }


      v8::Isolate* isolate;

      v8::Persistent<v8::Function> func;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
