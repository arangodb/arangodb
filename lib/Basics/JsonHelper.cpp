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
#include "Basics/StringUtils.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a JSON string or number into a uint64
////////////////////////////////////////////////////////////////////////////////

uint64_t JsonHelper::stringUInt64(TRI_json_t const* json) {
  if (json != nullptr) {
    if (json->_type == TRI_JSON_STRING) {
      return StringUtils::uint64(std::string(json->_value._string.data,
                                             json->_value._string.length - 1));
    } else if (json->_type == TRI_JSON_NUMBER) {
      return (uint64_t)json->_value._number;
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a JSON string or number into a uint64
////////////////////////////////////////////////////////////////////////////////

uint64_t JsonHelper::stringUInt64(TRI_json_t const* json, char const* name) {
  if (json == nullptr) {
    return 0;
  }

  TRI_json_t const* element = TRI_LookupObjectJson(json, name);
  return stringUInt64(element);
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

      if (TRI_IsStringJson(v)) {
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
  if (!TRI_IsObjectJson(json)) {
    return nullptr;
  }

  return TRI_LookupObjectJson(json, name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string element, or a default if it is does not exist
////////////////////////////////////////////////////////////////////////////////

std::string JsonHelper::getStringValue(TRI_json_t const* json,
                                       std::string const& defaultValue) {
  if (TRI_IsStringJson(json)) {
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

  if (TRI_IsStringJson(sub)) {
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

  if (TRI_IsBooleanJson(sub)) {
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

  if (!TRI_IsBooleanJson(sub)) {
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

  if (!TRI_IsStringJson(sub)) {
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

  if (!TRI_IsObjectJson(sub)) {
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

  if (!TRI_IsArrayJson(sub)) {
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
