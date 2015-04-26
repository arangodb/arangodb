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
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"
#include "Basics/JsonHelper.h"
#include "Basics/json-utilities.h"
#include "Basics/StringBuffer.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief register usage of an invalid function argument
////////////////////////////////////////////////////////////////////////////////
            
static void RegisterInvalidArgumentWarning (triagens::aql::Query* query,
                                            char const* functionName) {
  int code = TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH;

  std::string const& msg(triagens::basics::Exception::FillExceptionString(code, functionName));
  query->registerWarning(code, msg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract attribute names from the arguments
////////////////////////////////////////////////////////////////////////////////

static void ExtractKeys (std::unordered_set<std::string>& names,
                         triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const& parameters,
                         size_t startParameter,
                         char const* functionName) {  
  size_t const n = parameters.arraySize();

  for (size_t i = startParameter; i < n; ++i) {
    Json param(parameters.extractArrayMember(trx, collection, i, false));

    if (param.isString()) {
      TRI_json_t const* json = param.json();
      names.emplace(std::string(json->_value._string.data, json->_value._string.length - 1));
    }
    else if (param.isNumber()) {
      TRI_json_t const* json = param.json();
      double number = json->_value._number;

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } 
      else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(std::string(&buffer[0], static_cast<size_t>(length)));
      }
    }
    else if (param.isArray()) {
      TRI_json_t const* p = param.json();

      size_t const n2 = param.size();
      for (size_t j = 0; j < n2; ++j) {
        auto v = static_cast<TRI_json_t const*>(TRI_AtVector(&p->_value._objects, j));
        if (TRI_IsStringJson(v)) {
          names.emplace(std::string(v->_value._string.data, v->_value._string.length - 1));
        }
        else {
          RegisterInvalidArgumentWarning(query, functionName);
        }
      }
    }
  }
}

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

AqlValue Functions::IsNull (triagens::aql::Query*, 
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNull()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsBool (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isBoolean()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNumber (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNumber()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsString (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isString()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsArray (triagens::aql::Query*,
                             triagens::arango::AqlTransaction* trx,
                             TRI_document_collection_t const* collection,
                             AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isArray()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsObject (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isObject()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LENGTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Length (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));

  TRI_json_t const* json = j.json();
  size_t length = 0;

  if (json != nullptr) {
    switch (json->_type) {
      case TRI_JSON_UNUSED:
      case TRI_JSON_NULL: {
        length = 0;
        break;
      }

      case TRI_JSON_BOOLEAN: {
        length = (json->_value._boolean ? 1 : 0);
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

AqlValue Functions::Concat (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
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
  std::unique_ptr<TRI_json_t> j(TRI_CreateStringJson(TRI_UNKNOWN_MEM_ZONE, buffer.steal(), length));

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PASSTHRU
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Passthru (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {

  Json j(parameters.extractArrayMember(trx, collection, 0, true));
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.json());
  j.steal();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSET
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unset (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           TRI_document_collection_t const* collection,
                           AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET");
    return AqlValue(new Json(Json::Null));
  }
 
  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, collection, parameters, 1, "UNSET");


  // create result object
  TRI_json_t const* valueJson = value.json();
  size_t const n = valueJson->_value._objects._length;

  size_t size;
  if (names.size() >= n / 2) {
    size = 4; 
  }
  else {
    size = (n / 2) - names.size(); 
  }

  std::unique_ptr<TRI_json_t> j(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, size));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i + 1));

    if (TRI_IsStringJson(key) && 
        names.find(key->_value._string.data) == names.end()) {
      auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value);

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      } 

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, j.get(), key->_value._string.data, copy);
    }
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function KEEP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Keep (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          TRI_document_collection_t const* collection,
                          AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isObject()) {
    RegisterInvalidArgumentWarning(query, "KEEP");
    return AqlValue(new Json(Json::Null));
  }
 
  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, collection, parameters, 1, "KEEP");


  // create result object
  std::unique_ptr<TRI_json_t> j(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, names.size()));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = valueJson->_value._objects._length;

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i + 1));

    if (TRI_IsStringJson(key) && 
        names.find(key->_value._string.data) != names.end()) {
      auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value);

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      } 

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, j.get(), key->_value._string.data, copy);
    }
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MERGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Merge (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           TRI_document_collection_t const* collection,
                           AqlValue const parameters) {
  size_t const n = parameters.arraySize();

  if (n == 0) {
    // no parameters
    return AqlValue(new Json(Json::Object));
  }

  // use the first argument as the preliminary result
  Json initial(parameters.extractArrayMember(trx, collection, 0, true));

  if (! initial.isObject()) {
    RegisterInvalidArgumentWarning(query, "MERGE");
    return AqlValue(new Json(Json::Null));
  }

  std::unique_ptr<TRI_json_t> result(initial.steal());

  // now merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    Json param(parameters.extractArrayMember(trx, collection, i, false));

    if (! param.isObject()) {
      RegisterInvalidArgumentWarning(query, "MERGE");
      return AqlValue(new Json(Json::Null));
    }
 
    auto merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, result.get(), param.json(), false, true);

    if (merged == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    result.reset(merged);
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function HAS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Has (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const parameters) {
  size_t const n = parameters.arraySize();

  if (n < 2) {
    // no parameters
    return AqlValue(new Json(false));
  }
    
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isObject()) {
    // not an object
    return AqlValue(new Json(false));
  }
 
  // process name parameter 
  Json name(parameters.extractArrayMember(trx, collection, 1, false));

  char const* p;

  if (! name.isString()) {
    triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
    AppendAsString(buffer, name.json());
    p = buffer.c_str();
  }
  else {
    p = name.json()->_value._string.data;
  }
 
  bool const hasAttribute = (TRI_LookupObjectJson(value.json(), p) != nullptr);
  return AqlValue(new Json(hasAttribute));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
