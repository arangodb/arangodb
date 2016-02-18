////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/JsonHelper.h"
#include "Basics/conversions.h"
#include "Basics/StringBuffer.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64 into a JSON string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::uint64String(TRI_memory_zone_t* zone, uint64_t value) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace(value, (char*)&buffer);

  return TRI_CreateStringCopyJson(zone, buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a JSON strong or number into a uint64
////////////////////////////////////////////////////////////////////////////////

uint64_t JsonHelper::stringUInt64(TRI_json_t const* json) {
  if (json != nullptr) {
    if (json->_type == TRI_JSON_STRING) {
      return TRI_UInt64String2(json->_value._string.data,
                               json->_value._string.length - 1);
    } else if (json->_type == TRI_JSON_NUMBER) {
      return (uint64_t)json->_value._number;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a JSON strong or number into a uint64
////////////////////////////////////////////////////////////////////////////////

uint64_t JsonHelper::stringUInt64(TRI_json_t const* json, char const* name) {
  if (json == nullptr) {
    return 0;
  }

  TRI_json_t const* element = TRI_LookupObjectJson(json, name);
  return stringUInt64(element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a JSON key/value object from a key/value of strings
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::stringObject(
    TRI_memory_zone_t* zone, std::map<std::string, std::string> const& values) {
  TRI_json_t* json = TRI_CreateObjectJson(zone, values.size());

  if (json == nullptr) {
    return nullptr;
  }

  std::map<std::string, std::string>::const_iterator it;
  for (it = values.begin(); it != values.end(); ++it) {
    std::string const key = (*it).first;
    std::string const value = (*it).second;

    TRI_json_t* v = TRI_CreateStringCopyJson(zone, value.c_str(), value.size());
    if (v != nullptr) {
      TRI_Insert3ObjectJson(zone, json, key.c_str(), v);
    }
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a key/value object of strings from a JSON (sub-) object
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> JsonHelper::stringObject(
    TRI_json_t const* json) {
  std::map<std::string, std::string> result;

  if (isObject(json)) {
    size_t const n = TRI_LengthVectorJson(json);

    for (size_t i = 0; i < n; i += 2) {
      auto k = static_cast<TRI_json_t const*>(
          TRI_AtVector(&json->_value._objects, i));
      auto v = static_cast<TRI_json_t const*>(
          TRI_AtVector(&json->_value._objects, i + 1));

      if (isString(k) && isString(v)) {
        std::string const key =
            std::string(k->_value._string.data, k->_value._string.length - 1);
        std::string const value =
            std::string(v->_value._string.data, v->_value._string.length - 1);
        result.emplace(std::make_pair(key, value));
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a JSON object from an array of strings
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::stringArray(TRI_memory_zone_t* zone,
                                    std::vector<std::string> const& values) {
  TRI_json_t* json = TRI_CreateArrayJson(zone, values.size());

  if (json == nullptr) {
    return nullptr;
  }

  for (size_t i = 0, n = values.size(); i < n; ++i) {
    TRI_json_t* v =
        TRI_CreateStringCopyJson(zone, values[i].c_str(), values[i].size());

    if (v != nullptr) {
      TRI_PushBack3ArrayJson(zone, json, v);
    }
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an array of strings from a JSON (sub-) object
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> JsonHelper::stringArray(TRI_json_t const* json) {
  std::vector<std::string> result;

  if (isArray(json)) {
    size_t const n = TRI_LengthArrayJson(json);

    for (size_t i = 0; i < n; ++i) {
      auto v = static_cast<TRI_json_t const*>(
          TRI_AtVector(&json->_value._objects, i));

      if (isString(v)) {
        result.emplace_back(
            std::string(v->_value._string.data, v->_value._string.length - 1));
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create JSON from string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::fromString(std::string const& data) {
  return TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create JSON from string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::fromString(char const* data) {
  return TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create JSON from string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::fromString(char const* data, size_t length) {
  return TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify json
////////////////////////////////////////////////////////////////////////////////

std::string JsonHelper::toString(TRI_json_t const* json) {
  if (json == nullptr) {
    return "";
  }
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  int res = TRI_StringifyJson(&buffer, json);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyStringBuffer(&buffer);
    return "";
  }

  std::string out(TRI_BeginStringBuffer(&buffer),
                  TRI_LengthStringBuffer(&buffer));
  TRI_DestroyStringBuffer(&buffer);

  return out;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an object sub-element
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonHelper::getObjectElement(TRI_json_t const* json,
                                         char const* name) {
  if (!isObject(json)) {
    return nullptr;
  }

  return TRI_LookupObjectJson(json, name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string element, or a default if it is does not exist
////////////////////////////////////////////////////////////////////////////////

std::string JsonHelper::getStringValue(TRI_json_t const* json,
                                       std::string const& defaultValue) {
  if (isString(json)) {
    return std::string(json->_value._string.data,
                       json->_value._string.length - 1);
  }
  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or a default if it is does not exist
////////////////////////////////////////////////////////////////////////////////

std::string JsonHelper::getStringValue(TRI_json_t const* json, char const* name,
                                       std::string const& defaultValue) {
  TRI_json_t const* sub = getObjectElement(json, name);

  if (isString(sub)) {
    return std::string(sub->_value._string.data,
                       sub->_value._string.length - 1);
  }
  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default if it is does not exist
////////////////////////////////////////////////////////////////////////////////

bool JsonHelper::getBooleanValue(TRI_json_t const* json, char const* name,
                                 bool defaultValue) {
  TRI_json_t const* sub = getObjectElement(json, name);

  if (isBoolean(sub)) {
    return sub->_value._boolean;
  }

  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or throws an exception if the
/// sub-element does not exist or if it is not boolean
////////////////////////////////////////////////////////////////////////////////

bool JsonHelper::checkAndGetBooleanValue(TRI_json_t const* json,
                                         char const* name) {
  TRI_json_t const* sub = getObjectElement(json, name);

  if (!isBoolean(sub)) {
    std::string msg = "The attribute '" + std::string(name) +
                      "' was not found or is not a boolean.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }

  return sub->_value._boolean;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or throws if <name> does not exist
/// or it is not a string
////////////////////////////////////////////////////////////////////////////////

std::string JsonHelper::checkAndGetStringValue(TRI_json_t const* json,
                                               char const* name) {
  TRI_json_t const* sub = getObjectElement(json, name);

  if (!isString(sub)) {
    std::string msg = "The attribute '" + std::string(name) +
                      "' was not found or is not a string.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }
  return std::string(sub->_value._string.data, sub->_value._string.length - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an array sub-element, or throws an exception if the
/// sub-element does not exist or if it is not an object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t const* JsonHelper::checkAndGetObjectValue(TRI_json_t const* json,
                                                     char const* name) {
  TRI_json_t const* sub = getObjectElement(json, name);

  if (!isObject(sub)) {
    std::string msg = "The attribute '" + std::string(name) +
                      "' was not found or is not an object.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }

  return sub;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list sub-element, or throws an exception if the
/// sub-element does not exist or if it is not an array
////////////////////////////////////////////////////////////////////////////////

TRI_json_t const* JsonHelper::checkAndGetArrayValue(TRI_json_t const* json,
                                                    char const* name) {
  TRI_json_t const* sub = getObjectElement(json, name);

  if (!isArray(sub)) {
    std::string msg = "The attribute '" + std::string(name) +
                      "' was not found or is not an array.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, msg);
  }

  return sub;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON contents to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream, Json const* json) {
  stream << json->toString();
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON contents to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream, Json const& json) {
  stream << json.toString();
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON contents to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream, TRI_json_t const* json) {
  stream << JsonHelper::toString(json);
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON contents to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream, TRI_json_t const& json) {
  stream << JsonHelper::toString(&json);
  return stream;
}
