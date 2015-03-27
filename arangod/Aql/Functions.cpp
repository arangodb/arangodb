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
#include "Basics/fpconv.h"
#include "Basics/Exceptions.h"
#include "Basics/JsonHelper.h"
#include "Basics/StringBuffer.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON value to a string buffer
////////////////////////////////////////////////////////////////////////////////

static void AppendAsString (triagens::basics::StringBuffer& buffer,
                            TRI_json_t const* json) {
  TRI_json_type_e const type = (json == nullptr ? TRI_JSON_UNUSED : json->_type);

  switch (type) {
    case TRI_JSON_UNUSED: 
    case TRI_JSON_NULL: {
      buffer.appendText("null", strlen("null"));
      break;
    }
    case TRI_JSON_BOOLEAN: {
      if (json->_value._boolean) {
        buffer.appendText("true", strlen("true"));
      }
      else {
        buffer.appendText("false", strlen("false"));
      }
      break;
    }
    case TRI_JSON_NUMBER: {
      buffer.appendDecimal(json->_value._number);
      break;
    }
    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      buffer.appendText(json->_value._string.data, json->_value._string.length - 1);
      break;
    }
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthArrayJson(json);
      for (size_t i = 0; i < n; ++i) {
        if (i > 0) {
          buffer.appendChar(',');
        }
        AppendAsString(buffer, static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i)));
      }
      break;
    }
    case TRI_JSON_OBJECT: {
      buffer.appendText("[object Object]", strlen("[object Object]"));
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NULL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNull (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNull()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsBool (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isBoolean()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNumber (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNumber()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsString (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isString()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsArray (triagens::arango::AqlTransaction* trx,
                             TRI_document_collection_t const* collection,
                             AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isArray()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsObject (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isObject()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LENGTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Length (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));

  TRI_json_t const* json = j.json();
  size_t length = 0;

  if (json != nullptr) {
    switch (json->_type) {
      case TRI_JSON_UNUSED:
      case TRI_JSON_NULL: {
        length = strlen("null"); 
        break;
      }

      case TRI_JSON_BOOLEAN: {
        length = (json->_value._boolean ? strlen("true") : strlen("false"));
        break;
      }

      case TRI_JSON_NUMBER: {
        if (std::isnan(json->_value._number) ||
            ! std::isfinite(json->_value._number)) {
          // invalid value
          length = strlen("null");
        }
        else {
          // convert to a string representation of the number
          char buffer[24];
          length = static_cast<size_t>(fpconv_dtoa(json->_value._number, buffer));
        }
        break;
      }

      case TRI_JSON_STRING:
      case TRI_JSON_STRING_REFERENCE: {
        // return number of characters (not bytes) in string
        length = TRI_CharLengthUtf8String(json->_value._string.data);
        break;
      }

      case TRI_JSON_OBJECT: {
        // return number of attributes
        length = json->_value._objects._length / 2;
        break;
      }

      case TRI_JSON_ARRAY: {
        // return list length
        length = TRI_LengthArrayJson(json);
        break;
      }
    }
  }

  return AqlValue(new Json(static_cast<double>(length)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function CONCAT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Concat (triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);

  size_t const n = parameters.arraySize();

  for (size_t i = 0; i < n; ++i) {
    Json member = parameters.at(trx, i);

    if (member.isEmpty() || member.isNull()) {
      continue;
    }
      
    TRI_json_t const* json = member.json();
    
    if (member.isArray()) {
      // append each member individually
      size_t const subLength = TRI_LengthArrayJson(json);

      for (size_t j = 0; j < subLength; ++j) {
        auto sub = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, j));

        if (sub == nullptr || sub->_type == TRI_JSON_NULL) {
          continue;
        }

        AppendAsString(buffer, sub);
      }
    }
    else {
      // convert member to a string and append
      AppendAsString(buffer, json);
    }
  }
  
  // steal the StringBuffer's char* pointer so we can avoid copying data around
  // multiple times
  size_t length = buffer.length();
  TRI_json_t* j = TRI_CreateStringJson(TRI_UNKNOWN_MEM_ZONE, buffer.steal(), length);
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PASSTHRU
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Passthru (triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {

  Json j(parameters.extractArrayMember(trx, collection, 0, true));
  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
