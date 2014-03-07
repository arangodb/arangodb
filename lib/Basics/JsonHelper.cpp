////////////////////////////////////////////////////////////////////////////////
/// @brief json helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/JsonHelper.h"

#include "BasicsC/conversions.h"
#include "BasicsC/string-buffer.h"

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class JsonHelper
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64 into a JSON string 
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::uint64String (TRI_memory_zone_t* zone,
                                      uint64_t value) {
  char buffer[21];
  size_t len;

  len = TRI_StringUInt64InPlace(value, (char*) &buffer);

  return TRI_CreateString2CopyJson(zone, buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a JSON strong or number into a uint64
////////////////////////////////////////////////////////////////////////////////

uint64_t JsonHelper::stringUInt64 (TRI_json_t const* json) {
  if (json != 0) {
    if (json->_type == TRI_JSON_STRING) {
      return TRI_UInt64String2(json->_value._string.data, json->_value._string.length - 1);
    }
    else if (json->_type == TRI_JSON_NUMBER) {
      return (uint64_t) json->_value._number;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a JSON strong or number into a uint64
////////////////////////////////////////////////////////////////////////////////

uint64_t JsonHelper::stringUInt64 (TRI_json_t const* json,
                                   char const* name) {

  if (json == 0) {
    return 0;
  }

  TRI_json_t const* element = TRI_LookupArrayJson(json, name);
  return stringUInt64(element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a JSON key/value object from a list of strings
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::stringObject (TRI_memory_zone_t* zone,
                                      std::map<std::string, std::string> const& values) {
  TRI_json_t* json = TRI_CreateArray2Json(zone, values.size());

  if (json == 0) {
    return 0;
  }

  std::map<std::string, std::string>::const_iterator it;
  for (it = values.begin(); it != values.end(); ++it) {
    const std::string key = (*it).first;
    const std::string value = (*it).second;

    TRI_json_t* v = TRI_CreateString2CopyJson(zone, value.c_str(), value.size());
    if (v != 0) {
      TRI_Insert3ArrayJson(zone, json, key.c_str(), v);
    }
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a key/value object of strings from a JSON (sub-) object
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> JsonHelper::stringObject (TRI_json_t const* json) {
  std::map<std::string, std::string> result;

  if (isArray(json)) {
    for (size_t i = 0, n = json->_value._objects._length; i < n; i += 2) {
      TRI_json_t const* k = (TRI_json_t const*) TRI_AtVector(&json->_value._objects, i);
      TRI_json_t const* v = (TRI_json_t const*) TRI_AtVector(&json->_value._objects, i + 1);

      if (isString(k) && isString(v)) {
        const std::string key = std::string(k->_value._string.data, k->_value._string.length - 1);
        const std::string value = std::string(v->_value._string.data, v->_value._string.length - 1);
        result.insert(std::pair<std::string, std::string>(key, value));
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a JSON object from a list of strings
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::stringList (TRI_memory_zone_t* zone,
                                    std::vector<std::string> const& values) {
  TRI_json_t* json = TRI_CreateList2Json(zone, values.size());

  if (json == 0) {
    return 0;
  }

  for (size_t i = 0, n = values.size(); i < n; ++i) {
    TRI_json_t* v = TRI_CreateString2CopyJson(zone, values[i].c_str(), values[i].size());
    if (v != 0) {
      TRI_PushBack3ListJson(zone, json, v);
    }
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a list of strings from a JSON (sub-) object
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> JsonHelper::stringList (TRI_json_t const* json) {
  std::vector<std::string> result;

  if (isList(json)) {
    for (size_t i = 0, n = json->_value._objects._length; i < n; ++i) {
      TRI_json_t const* v = (TRI_json_t const*) TRI_AtVector(&json->_value._objects, i);

      if (isString(v)) {
        result.push_back(std::string(v->_value._string.data, v->_value._string.length - 1));
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create JSON from string
////////////////////////////////////////////////////////////////////////////////
        
TRI_json_t* JsonHelper::fromString (std::string const& data) {
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data.c_str());

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create JSON from string
////////////////////////////////////////////////////////////////////////////////
        
TRI_json_t* JsonHelper::fromString (char const* data,
                                    size_t length) {
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data);

  return json;
}
      
////////////////////////////////////////////////////////////////////////////////
/// @brief stringify json
////////////////////////////////////////////////////////////////////////////////
        
std::string JsonHelper::toString (TRI_json_t const* json) {
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  int res = TRI_StringifyJson(&buffer, json);

  if (res != TRI_ERROR_NO_ERROR) {
    return "";
  }

  string out(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));
  TRI_DestroyStringBuffer(&buffer);

  return out;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an array sub-element
////////////////////////////////////////////////////////////////////////////////
        
TRI_json_t* JsonHelper::getArrayElement (TRI_json_t const* json, 
                                         const char* name) {
  if (! isArray(json)) {
    return 0;
  }

  return TRI_LookupArrayJson(json, name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
std::string JsonHelper::getStringValue (TRI_json_t const* json, 
                                        const std::string& defaultValue) {
  if (isString(json)) {
    return string(json->_value._string.data, json->_value._string.length - 1);
  }
  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
std::string JsonHelper::getStringValue (TRI_json_t const* json, 
                                        const char* name, 
                                        const std::string& defaultValue) {
  TRI_json_t const* sub = getArrayElement(json, name);

  if (isString(sub)) {
    return string(sub->_value._string.data, sub->_value._string.length - 1);
  }
  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default it is does not exist
////////////////////////////////////////////////////////////////////////////////
        
bool JsonHelper::getBooleanValue (TRI_json_t const* json, 
                                  const char* name, 
                                  bool defaultValue) {
  TRI_json_t const* sub = getArrayElement(json, name);

  if (isBoolean(sub)) {
    return sub->_value._boolean;
  }

  return defaultValue;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
