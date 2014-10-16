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

#ifndef ARANGODB_AQL_V8_EXPRESSION_H
#define ARANGODB_AQL_V8_EXPRESSION_H 1

#include "Basics/Common.h"
#include "Aql/AqlValue.h"
#include "Aql/Types.h"
#include <v8.h>

namespace triagens {
  namespace aql {

    struct Variable;

// -----------------------------------------------------------------------------
// --SECTION--                                               struct V8Expression
// -----------------------------------------------------------------------------

    struct V8Expression {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the v8 expression
////////////////////////////////////////////////////////////////////////////////

      V8Expression (v8::Isolate*,
                    v8::Persistent<v8::Function>);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the v8 expression
////////////////////////////////////////////////////////////////////////////////

      ~V8Expression ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

      AqlValue execute (triagens::arango::AqlTransaction*,
                        std::vector<TRI_document_collection_t const*>&,
                        std::vector<AqlValue>&,
                        size_t,
                        std::vector<Variable*> const&,
                        std::vector<RegisterId> const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

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
