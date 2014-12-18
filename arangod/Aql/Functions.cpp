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
  Json j(parameters.extractListMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNull()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsBool (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isBoolean()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNumber (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNumber()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsString (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isString()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsArray (triagens::arango::AqlTransaction* trx,
                             TRI_document_collection_t const* collection,
                             AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isArray()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsObject (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isObject()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LENGTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Length (triagens::arango::AqlTransaction* trx,
                                TRI_document_collection_t const* collection,
                                AqlValue const parameters) {
  Json j(parameters.extractListMember(trx, collection, 0, false));

  TRI_json_t const* json = j.json();
  size_t length = 0;

  if (json != nullptr) {
    switch (json->_type) {
      case TRI_JSON_UNUSED:
      case TRI_JSON_NULL:
        length = strlen("null"); 
        break;

      case TRI_JSON_BOOLEAN:
        length = (json->_value._boolean ? strlen("true") : strlen("false"));
        break;

      case TRI_JSON_NUMBER:
      case TRI_JSON_STRING:
      case TRI_JSON_STRING_REFERENCE:
        // return number of characters (not byteS) in string
        length = TRI_CharLengthUtf8String(json->_value._string.data);
        break;

      case TRI_JSON_OBJECT:
        // return number of attributes
        length = json->_value._objects._length / 2;
        break;

      case TRI_JSON_ARRAY:
        // return list length
        length = TRI_LengthArrayJson(json);
        break;
    }
  }

  return AqlValue(new Json(static_cast<double>(length)));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
