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

#ifndef LIB_BASICS_JSON_UTILITIES_H
#define LIB_BASICS_JSON_UTILITIES_H 1

#include "Basics/Common.h"
#include "Basics/json.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two json values
///
/// the values are first compared by their types, and only by their values if
/// the types are the same
/// returns -1 if lhs is smaller than rhs, 0 if lhs == rhs, and 1 if rhs is
/// greater than lhs.
///
/// If useUTF8 is set to true, strings will be converted using proper UTF8,
/// otherwise, strcmp is used, so it should only be used to test for equality
/// in this case.
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareValuesJson(TRI_json_t const*, TRI_json_t const*,
                          bool useUTF8 = true);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if two json values are the same
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckSameValueJson(TRI_json_t const*, TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief uniquify a sorted json list into a new array
///
/// it is a prerequisite that the input list is already sorted.
/// otherwise the result is unpredictable.
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UniquifyArrayJson(TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a json array in place
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_SortArrayJson(TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a JSON struct has duplicate attribute names
////////////////////////////////////////////////////////////////////////////////

bool TRI_HasDuplicateKeyJson(TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two JSON documents into one
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_MergeJson(TRI_memory_zone_t*, TRI_json_t const*,
                          TRI_json_t const*, bool, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document.
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashJson(TRI_json_t const* json);

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document, using fasthash64.
/// This is slightly faster than the FNV-based hashing
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FastHashJson(TRI_json_t const* json);

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document depending on a list
/// of attributes.
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashJsonByAttributes(TRI_json_t const* json,
                                  char const* attributes[], int nrAttributes,
                                  bool docComplete, int* error);

////////////////////////////////////////////////////////////////////////////////
/// @brief hasher for JSON value
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace basics {

struct JsonHash {
  inline size_t operator()(TRI_json_t const* value) const {
    return TRI_FastHashJson(value);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief equality comparator for JSON values
////////////////////////////////////////////////////////////////////////////////

struct JsonEqual {
  inline bool operator()(TRI_json_t const* lhs, TRI_json_t const* rhs) const {
    return (TRI_CompareValuesJson(lhs, rhs, false) == 0);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief less comparator for JSON values
////////////////////////////////////////////////////////////////////////////////

template <bool useUtf8>
struct JsonLess {
  inline bool operator()(TRI_json_t const* lhs, TRI_json_t const* rhs) const {
    return TRI_CompareValuesJson(lhs, rhs, useUtf8) < 0;
  }
};
}
}

#endif
