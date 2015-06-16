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
#include "Basics/tri-strings.h"
#include "Utils/AqlTransaction.h"

#include <functional>
      
namespace triagens {
  namespace aql {

    class Query;

// -----------------------------------------------------------------------------
// --SECTION--                                                   public typedefs
// -----------------------------------------------------------------------------

    typedef AqlValue const FunctionParameters;

    typedef std::function<AqlValue(triagens::aql::Query*,
                                   triagens::arango::AqlTransaction*,
                                   TRI_document_collection_t const*,
                                   FunctionParameters)> FunctionImplementation;

// -----------------------------------------------------------------------------
// --SECTION--                                             AQL function bindings
// -----------------------------------------------------------------------------

    struct Functions {

      static AqlValue IsNull        (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue IsBool        (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue IsNumber      (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue IsString      (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue IsArray       (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue IsObject      (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Length        (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Concat        (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Passthru      (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Unset         (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Keep          (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Merge         (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Has           (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Attributes    (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Values        (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Min           (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Max           (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Sum           (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Average       (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Md5           (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Sha1          (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Unique        (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Union         (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue UnionDistinct (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
      static AqlValue Intersection  (triagens::aql::Query*, triagens::arango::AqlTransaction*, TRI_document_collection_t const*, FunctionParameters);
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
