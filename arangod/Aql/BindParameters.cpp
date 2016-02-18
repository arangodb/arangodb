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

#include "Aql/BindParameters.h"
#include "Basics/json.h"
#include "Basics/json-utilities.h"
#include "Basics/Exceptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the parameters
////////////////////////////////////////////////////////////////////////////////

BindParameters::BindParameters(TRI_json_t* json)
    : _json(json), _parameters(), _processed(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the parameters
////////////////////////////////////////////////////////////////////////////////

BindParameters::~BindParameters() {
  if (_json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a hash value for the bind parameters
////////////////////////////////////////////////////////////////////////////////

uint64_t BindParameters::hash() const {
  if (_json == nullptr) {
    return 0x12345678abcdef;
  }

  return TRI_FastHashJson(_json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the parameters
////////////////////////////////////////////////////////////////////////////////

void BindParameters::process() {
  if (_processed || _json == nullptr) {
    return;
  }

  if (!TRI_IsObjectJson(_json)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID);
  }

  size_t const n = TRI_LengthVector(&_json->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(
        TRI_AddressVector(&_json->_value._objects, i));

    if (!TRI_IsStringJson(key)) {
      // no string, should not happen
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE);
    }

    std::string const k(key->_value._string.data,
                        key->_value._string.length - 1);

    auto value = static_cast<TRI_json_t const*>(
        TRI_AtVector(&_json->_value._objects, i + 1));

    if (value == nullptr) {
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE,
                                    k.c_str());
    }

    if (k[0] == '@' && !TRI_IsStringJson(value)) {
      // collection bind parameter
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE,
                                    k.c_str());
    }

    _parameters.emplace(std::move(k), std::make_pair(value, false));
  }

  _processed = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief strip collection name prefixes from the parameters
/// the values must be a VelocyPack array
////////////////////////////////////////////////////////////////////////////////

VPackBuilder BindParameters::StripCollectionNames(VPackSlice const& keys,
                                                  char const* collectionName) {
  TRI_ASSERT(keys.isArray());
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Array));
  for (auto const& element : VPackArrayIterator(keys)) {
    if (element.isString()) {
      VPackValueLength l;
      char const* s = element.getString(l);
      char search = '/';
      auto p = static_cast<char const*>(memchr(s, search, l));
      if (p != nullptr && strncmp(s, collectionName, p - s) == 0) {
        // key begins with collection name + '/', now strip it in place for
        // further comparisons
        result.add(VPackValue(
            std::string(p + 1, l - static_cast<ptrdiff_t>(p - s) - 1)));
        continue;
      }
    }
    result.add(element);
  }
  result.close();
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief strip collection name prefixes from the parameters
/// the values must be a JSON array. the array is modified in place
////////////////////////////////////////////////////////////////////////////////

void BindParameters::StripCollectionNames(TRI_json_t* keys,
                                          char const* collectionName) {
  if (!TRI_IsArrayJson(keys)) {
    return;
  }

  size_t const n = TRI_LengthVectorJson(keys);

  for (size_t i = 0; i < n; ++i) {
    auto key =
        static_cast<TRI_json_t*>(TRI_AtVector(&keys->_value._objects, i));

    if (TRI_IsStringJson(key)) {
      auto s = key->_value._string.data;
      auto p = strchr(s, '/');

      if (p != nullptr && strncmp(s, collectionName, p - s) == 0) {
        size_t pos = static_cast<size_t>(p - s);
        // key begins with collection name + '/', now strip it in place for
        // further comparisons
        memmove(s, p + 1, key->_value._string.length - 2 - pos);
        key->_value._string.length -= static_cast<uint32_t>(pos + 1);
        key->_value._string.data[key->_value._string.length - 1] = '\0';
      }
    }
  }
}
