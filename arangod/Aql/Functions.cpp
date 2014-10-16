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

#include "Aql/Functions.h"
#include "Basics/JsonHelper.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NULL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNull (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0));
  return AqlValue(new Json(j.isNull()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsBool (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0));
  return AqlValue(new Json(j.isBoolean()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNumber (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0));
  return AqlValue(new Json(j.isNumber()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsString (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0));
  return AqlValue(new Json(j.isString()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_LIST
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsList (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0));
  return AqlValue(new Json(j.isList()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_DOCUMENT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsDocument (triagens::arango::AqlTransaction* trx,
                                TRI_document_collection_t const* collection,
                                AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0));
  return AqlValue(new Json(j.isArray()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
