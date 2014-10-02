////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, C++ implementation of AQL functions
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

#ifndef ARANGODB_AQL_FUNCTIONS_H
#define ARANGODB_AQL_FUNCTIONS_H 1

#include "Basics/Common.h"
#include "Aql/AqlValue.h"

#include <functional>

namespace triagens {
  namespace aql {

    typedef std::function<AqlValue(AQL_TRANSACTION_V8*,
                                   TRI_document_collection_t const*,
                                   AqlValue const)> FunctionImplementation;

    struct Functions {

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NULL
////////////////////////////////////////////////////////////////////////////////

      static AqlValue IsNull (AQL_TRANSACTION_V8*, TRI_document_collection_t const*, AqlValue const);
      static AqlValue IsBool (AQL_TRANSACTION_V8*, TRI_document_collection_t const*, AqlValue const);
      static AqlValue IsNumber (AQL_TRANSACTION_V8*, TRI_document_collection_t const*, AqlValue const);
      static AqlValue IsString (AQL_TRANSACTION_V8*, TRI_document_collection_t const*, AqlValue const);
      static AqlValue IsList (AQL_TRANSACTION_V8*, TRI_document_collection_t const*, AqlValue const);
      static AqlValue IsDocument (AQL_TRANSACTION_V8*, TRI_document_collection_t const*, AqlValue const);
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
