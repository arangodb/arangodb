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

      v8::Handle<v8::Value> execute (v8::Handle<v8::Value> argv) {
        v8::HandleScope scope;

        v8::Handle<v8::Value> args[] = { argv };
        return scope.Close(func->Call(func, 1, args));
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
