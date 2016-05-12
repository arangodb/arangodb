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

#include "json-utilities.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

static TRI_json_t* MergeRecursive(TRI_memory_zone_t* zone,
                                  TRI_json_t const* lhs, TRI_json_t const* rhs,
                                  bool nullMeansRemove, bool mergeObjects) {
  TRI_ASSERT(lhs != nullptr);

  std::unique_ptr<TRI_json_t> result(TRI_CopyJson(zone, lhs));

  if (result == nullptr) {
    return nullptr;
  }

  auto r = result.get();  // shortcut variable

  size_t const n = TRI_LengthVector(&rhs->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    // enumerate all the replacement values
    auto key =
        static_cast<TRI_json_t const*>(TRI_AtVector(&rhs->_value._objects, i));
    auto value = static_cast<TRI_json_t const*>(
        TRI_AtVector(&rhs->_value._objects, i + 1));

    if (value->_type == TRI_JSON_NULL && nullMeansRemove) {
      // replacement value is a null and we don't want to store nulls => delete
      // attribute from the result
      TRI_DeleteObjectJson(zone, r, key->_value._string.data);
    } else {
      // replacement value is not a null or we want to store nulls
      TRI_json_t const* lhsValue =
          TRI_LookupObjectJson(lhs, key->_value._string.data);

      if (lhsValue == nullptr) {
        // existing array does not have the attribute => append new attribute
        if (value->_type == TRI_JSON_OBJECT && nullMeansRemove) {
          TRI_json_t empty;
          TRI_InitObjectJson(TRI_UNKNOWN_MEM_ZONE, &empty);
          TRI_json_t* merged = MergeRecursive(zone, &empty, value,
                                              nullMeansRemove, mergeObjects);

          if (merged == nullptr) {
            return nullptr;
          }
          TRI_Insert3ObjectJson(zone, r, key->_value._string.data, merged);
        } else {
          TRI_json_t* copy = TRI_CopyJson(zone, value);

          if (copy == nullptr) {
            return nullptr;
          }

          TRI_Insert3ObjectJson(zone, r, key->_value._string.data, copy);
        }
      } else {
        // existing array already has the attribute => replace attribute
        if (lhsValue->_type == TRI_JSON_OBJECT &&
            value->_type == TRI_JSON_OBJECT && mergeObjects) {
          TRI_json_t* merged = MergeRecursive(zone, lhsValue, value,
                                              nullMeansRemove, mergeObjects);
          if (merged == nullptr) {
            return nullptr;
          }
          TRI_ReplaceObjectJson(zone, r, key->_value._string.data, merged);
          TRI_FreeJson(zone, merged);
        } else {
          TRI_ReplaceObjectJson(zone, r, key->_value._string.data, value);
        }
      }
    }
  }

  return result.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get type weight of a json value usable for comparison and sorting
////////////////////////////////////////////////////////////////////////////////

static int TypeWeight(TRI_json_t const* value) {
  if (value != nullptr) {
    switch (value->_type) {
      case TRI_JSON_BOOLEAN:
        return 1;
      case TRI_JSON_NUMBER:
        return 2;
      case TRI_JSON_STRING:
      case TRI_JSON_STRING_REFERENCE:
        // a string reference has the same weight as a regular string
        return 3;
      case TRI_JSON_ARRAY:
        return 4;
      case TRI_JSON_OBJECT:
        return 5;
      case TRI_JSON_NULL:
      case TRI_JSON_UNUSED:
        break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief uniquify a sorted json list into a new array
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* UniquifyArrayJson(TRI_json_t const* array) {
  TRI_ASSERT(array != nullptr);
  TRI_ASSERT(array->_type == TRI_JSON_ARRAY);

  // create result array
  std::unique_ptr<TRI_json_t> result(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE));

  if (result == nullptr) {
    return nullptr;
  }

  size_t const n = TRI_LengthVector(&array->_value._objects);

  TRI_json_t const* last = nullptr;
  for (size_t i = 0; i < n; ++i) {
    auto p = static_cast<TRI_json_t const*>(
        TRI_AtVector(&array->_value._objects, i));

    // don't push value if it is the same as the last value
    if (last == nullptr || TRI_CompareValuesJson(p, last, false) != 0) {
      TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), p);

      // remember last element
      last = p;
    }
  }

  return result.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function used for json value sorting
////////////////////////////////////////////////////////////////////////////////

static int CompareJson(void const* lhs, void const* rhs) {
  return TRI_CompareValuesJson(static_cast<TRI_json_t const*>(lhs),
                               static_cast<TRI_json_t const*>(rhs), true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a json array in place
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* SortArrayJson(TRI_json_t* array) {
  TRI_ASSERT(array != nullptr);
  TRI_ASSERT(array->_type == TRI_JSON_ARRAY);

  size_t const n = TRI_LengthVector(&array->_value._objects);

  if (n > 1) {
    // only sort if more than one value in array
    qsort(TRI_BeginVector(&array->_value._objects), n, sizeof(TRI_json_t),
          &CompareJson);
  }

  return array;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two arrays of array keys, sort them and return a combined array
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* GetMergedKeyArray(TRI_json_t const* lhs,
                                     TRI_json_t const* rhs) {
  TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);

  size_t n = TRI_LengthVector(&lhs->_value._objects) +
             TRI_LengthVector(&rhs->_value._objects);

  std::unique_ptr<TRI_json_t> keys(
      TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, n));

  if (keys == nullptr) {
    return nullptr;
  }

  if (TRI_CapacityVector(&(keys.get()->_value._objects)) < n) {
    return nullptr;
  }

  n = TRI_LengthVector(&lhs->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto key =
        static_cast<TRI_json_t const*>(TRI_AtVector(&lhs->_value._objects, i));

    TRI_ASSERT(TRI_IsStringJson(key));
    TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, keys.get(), key);
  }

  n = TRI_LengthVector(&rhs->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto key =
        static_cast<TRI_json_t const*>(TRI_AtVector(&rhs->_value._objects, i));

    TRI_ASSERT(TRI_IsStringJson(key));
    TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, keys.get(), key);
  }

  // sort the key array in place
  SortArrayJson(keys.get());

  // array is now sorted
  return UniquifyArrayJson(keys.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two json values
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareValuesJson(TRI_json_t const* lhs, TRI_json_t const* rhs,
                          bool useUTF8) {
  // note: both lhs and rhs may be NULL!
  {
    int lWeight = TypeWeight(lhs);
    int rWeight = TypeWeight(rhs);

    if (lWeight < rWeight) {
      return -1;
    }

    if (lWeight > rWeight) {
      return 1;
    }

    TRI_ASSERT(lWeight == rWeight);
  }

  // lhs and rhs have equal weights

  if (lhs == nullptr || rhs == nullptr) {
    // either lhs or rhs is a nullptr. we cannot be sure here that both are
    // nullptrs.
    // there can also exist the situation that lhs is a nullptr and rhs is a
    // JSON null value
    // (or vice versa). Anyway, the compare value is the same for both,
    return 0;
  }

  switch (lhs->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL: {
      return 0;  // null == null;
    }

    case TRI_JSON_BOOLEAN: {
      if (lhs->_value._boolean == rhs->_value._boolean) {
        return 0;
      }

      if (!lhs->_value._boolean && rhs->_value._boolean) {
        return -1;
      }

      return 1;
    }

    case TRI_JSON_NUMBER: {
      if (lhs->_value._number == rhs->_value._number) {
        return 0;
      }

      if (lhs->_value._number < rhs->_value._number) {
        return -1;
      }

      return 1;
    }

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      // same for STRING and STRING_REFERENCE
      TRI_ASSERT(lhs->_value._string.data != nullptr);
      TRI_ASSERT(rhs->_value._string.data != nullptr);
      int res;
      size_t const nl = lhs->_value._string.length - 1;
      size_t const nr = rhs->_value._string.length - 1;
      if (useUTF8) {
        res = TRI_compare_utf8(lhs->_value._string.data, nl,
                               rhs->_value._string.data, nr);
      } else {
        // beware of strings containing NUL bytes
        size_t len = nl < nr ? nl : nr;
        res = memcmp(lhs->_value._string.data, rhs->_value._string.data, len);
      }
      if (res < 0) {
        return -1;
      } else if (res > 0) {
        return 1;
      }
      // res == 0
      if (nl == nr) {
        return 0;
      }
      // res == 0, but different string lengths
      return nl < nr ? -1 : 1;
    }

    case TRI_JSON_ARRAY: {
      size_t const nl = TRI_LengthVector(&lhs->_value._objects);
      size_t const nr = TRI_LengthVector(&rhs->_value._objects);
      size_t n;

      if (nl > nr) {
        n = nl;
      } else {
        n = nr;
      }

      for (size_t i = 0; i < n; ++i) {
        auto lhsValue =
            (i >= nl) ? nullptr : static_cast<TRI_json_t const*>(
                                      TRI_AtVector(&lhs->_value._objects, i));
        auto rhsValue =
            (i >= nr) ? nullptr : static_cast<TRI_json_t const*>(
                                      TRI_AtVector(&rhs->_value._objects, i));

        int result = TRI_CompareValuesJson(lhsValue, rhsValue, useUTF8);

        if (result != 0) {
          return result;
        }
      }

      return 0;
    }

    case TRI_JSON_OBJECT: {
      TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
      TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);

      std::unique_ptr<TRI_json_t> keys(GetMergedKeyArray(lhs, rhs));

      if (keys != nullptr) {
        auto json = keys.get();
        size_t const n = TRI_LengthVector(&json->_value._objects);

        for (size_t i = 0; i < n; ++i) {
          auto keyElement = static_cast<TRI_json_t const*>(
              TRI_AtVector(&json->_value._objects, i));
          TRI_ASSERT(TRI_IsStringJson(keyElement));

          TRI_json_t const* lhsValue = TRI_LookupObjectJson(
              lhs, keyElement->_value._string.data);  // may be NULL
          TRI_json_t const* rhsValue = TRI_LookupObjectJson(
              rhs, keyElement->_value._string.data);  // may be NULL

          int result = TRI_CompareValuesJson(lhsValue, rhsValue, useUTF8);

          if (result != 0) {
            return result;
          }
        }
      }
      // fall-through to returning 0
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two JSON documents into one
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_MergeJson(TRI_memory_zone_t* zone, TRI_json_t const* lhs,
                          TRI_json_t const* rhs, bool nullMeansRemove,
                          bool mergeObjects) {
  TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);

  return MergeRecursive(zone, lhs, rhs, nullMeansRemove, mergeObjects);
}

