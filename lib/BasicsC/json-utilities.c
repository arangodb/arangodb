////////////////////////////////////////////////////////////////////////////////
/// @brief utility functions for json objects
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/json-utilities.h>
#include <BasicsC/string-buffer.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* MergeRecursive (TRI_memory_zone_t* zone, 
                                   const TRI_json_t* const lhs, 
                                   const TRI_json_t* const rhs,
                                   const bool nullMeansRemove) {
  size_t i, n;
  
  TRI_json_t* result = TRI_CopyJson(zone, lhs);

  if (result == NULL) {
    return NULL;
  }

  n = rhs->_value._objects._length;
  for (i = 0; i < n; i += 2) {
    // enumerate all the replacement values
    TRI_json_t* key = TRI_AtVector(&rhs->_value._objects, i);
    TRI_json_t* value = TRI_AtVector(&rhs->_value._objects, i + 1);

    if (value->_type == TRI_JSON_NULL && nullMeansRemove) {
      // replacement value is a null and we don't want to store nulls => delete attribute from the result
      TRI_DeleteArrayJson(zone, result, key->_value._string.data);
    }
    else {
      // replacement value is not a null or we want to store nulls
      TRI_json_t* lhsValue = TRI_LookupArrayJson(lhs, key->_value._string.data);

      if (lhsValue == NULL) {
        // existing array does not have the attribute => append new attribute
        if (value->_type == TRI_JSON_ARRAY) {
          TRI_json_t* empty = TRI_CreateArrayJson(zone);
          TRI_json_t* merged = MergeRecursive(zone, empty, value, nullMeansRemove);
          TRI_Insert3ArrayJson(zone, result, key->_value._string.data, merged);

          TRI_FreeJson(zone, empty);
        }
        else {
          TRI_Insert3ArrayJson(zone, result, key->_value._string.data, TRI_CopyJson(zone, value));
        }
      }
      else {
        // existing array already has the attribute => replace attribute
        if (lhsValue->_type == TRI_JSON_ARRAY && value->_type == TRI_JSON_ARRAY) {
          TRI_json_t* merged = MergeRecursive(zone, lhsValue, value, nullMeansRemove);
          TRI_ReplaceArrayJson(zone, result, key->_value._string.data, merged);
          TRI_FreeJson(zone, merged);
        }
        else {
          TRI_ReplaceArrayJson(zone, result, key->_value._string.data, value);
        }
      }
    }

  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fruitsort initialisation parameters
///
/// Included fsrt.inc with these parameters will create a function SortJsonList
/// that is used to do the sorting. SortJsonList will call OrderDataCompareFunc()
/// to do the actual element comparisons
////////////////////////////////////////////////////////////////////////////////

static int CompareJson (TRI_json_t*, TRI_json_t*, size_t);

#define FSRT_INSTANCE SortJson
#define FSRT_NAME SortListJson
#define FSRT_TYPE TRI_json_t
#define FSRT_COMP(l,r,s) CompareJson(l,r,s)

uint32_t SortJsonFSRT_Rand = 0;

static uint32_t SortJsonRandomGenerator (void) {
  return (SortJsonFSRT_Rand = SortJsonFSRT_Rand * 31415 + 27818);
}

#define FSRT__RAND ((fs_b) + FSRT__UNIT * (SortJsonRandomGenerator() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include "BasicsC/fsrt.inc"

#undef FSRT__RAND

////////////////////////////////////////////////////////////////////////////////
/// @brief get type weight of a json value usable for comparison and sorting
////////////////////////////////////////////////////////////////////////////////

static int TypeWeight (const TRI_json_t* const value) {
  if (value == NULL) {
    return 0;
  }

  switch (value->_type) {
    case TRI_JSON_BOOLEAN:
      return 1;
    case TRI_JSON_NUMBER:
      return 2;
    case TRI_JSON_STRING:
      return 3;
    case TRI_JSON_LIST:
      return 4;
    case TRI_JSON_ARRAY:
      return 5;
    case TRI_JSON_NULL:
    case TRI_JSON_UNUSED:
    default: 
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function used for json value sorting
////////////////////////////////////////////////////////////////////////////////

static int CompareJson (TRI_json_t* lhs, TRI_json_t* rhs, size_t size) {
  return TRI_CompareValuesJson((TRI_json_t*) lhs, (TRI_json_t*) rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two list of array keys, sort them and return a combined list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* GetMergedKeyList (const TRI_json_t* const lhs, 
                                     const TRI_json_t* const rhs) {
  TRI_json_t* keys;
  TRI_json_t* unique;
  size_t i, n;

  assert(lhs->_type == TRI_JSON_ARRAY);
  assert(rhs->_type == TRI_JSON_ARRAY);

  keys = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (keys == NULL) {
    return NULL;
  }

  n = lhs->_value._objects._length;
  for (i = 0 ; i < n; i += 2) {
    TRI_json_t* key = TRI_AtVector(&lhs->_value._objects, i); 

    assert(key->_type == TRI_JSON_STRING);
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, keys, key); 
  } 

  n = rhs->_value._objects._length;
  for (i = 0 ; i < n; i += 2) {
    TRI_json_t* key = TRI_AtVector(&rhs->_value._objects, i); 

    assert(key->_type == TRI_JSON_STRING);
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, keys, key); 
  } 

  // sort the key list in place
  TRI_SortListJson(keys);

  // list is now sorted
  unique = TRI_UniquifyListJson(keys);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);

  return unique; // might be NULL
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two json values
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareValuesJson (const TRI_json_t* const lhs, 
                           const TRI_json_t* const rhs) {
  // note: both lhs and rhs may be NULL!
  int lWeight = TypeWeight(lhs);
  int rWeight = TypeWeight(rhs);
  
  if (lWeight < rWeight) {
    return -1;
  }

  if (lWeight > rWeight) {
    return 1;
  }

  // lhs and rhs have equal weights
  if (lhs == NULL) {
    // both lhs and rhs are NULL, so they are equal
    return 0;
  }

  switch (lhs->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL:
      return 0; // null == null;

    case TRI_JSON_BOOLEAN:
      if (lhs->_value._boolean == rhs->_value._boolean) {
        return 0;
      }

      if (!lhs->_value._boolean && rhs->_value._boolean) {
        return -1;
      }

      return 1;

    case TRI_JSON_NUMBER:
      if (lhs->_value._number == rhs->_value._number) {
        return 0;
      }

      if (lhs->_value._number < rhs->_value._number) {
        return -1;
      }

      return 1;

    case TRI_JSON_STRING:
      return strcmp(lhs->_value._string.data, rhs->_value._string.data);

    case TRI_JSON_LIST: {
      size_t nl = lhs->_value._objects._length;
      size_t nr = rhs->_value._objects._length;
      size_t n;
      size_t i;

      if (nl > nr) {
        n = nl;
      }
      else {
        n = nr;
      }

      for (i = 0; i < n; ++i) {
        TRI_json_t* lhsValue;
        TRI_json_t* rhsValue;
        int result;

        lhsValue = (i >= nl) ? NULL : TRI_AtVector(&lhs->_value._objects, i);
        rhsValue = (i >= nr) ? NULL : TRI_AtVector(&rhs->_value._objects, i);
        result = TRI_CompareValuesJson(lhsValue, rhsValue);
        if (result != 0) {
          return result;
        }
      }

      return 0;
    }

    case TRI_JSON_ARRAY: {
      TRI_json_t* keys;
          
      assert(lhs->_type == TRI_JSON_ARRAY);
      assert(rhs->_type == TRI_JSON_ARRAY);

      keys = GetMergedKeyList(lhs, rhs);
      if (keys != NULL) {
        size_t i, n;

        n = keys->_value._objects._length;
        for (i = 0; i < n; ++i) {
          TRI_json_t* keyElement;
          TRI_json_t* lhsValue;
          TRI_json_t* rhsValue;
          int result;

          keyElement = TRI_AtVector(&keys->_value._objects, i);
          assert(keyElement->_type == TRI_JSON_STRING);
          assert(keyElement->_value._string.data);

          lhsValue = TRI_LookupArrayJson((TRI_json_t*) lhs, keyElement->_value._string.data); // may be NULL
          rhsValue = TRI_LookupArrayJson((TRI_json_t*) rhs, keyElement->_value._string.data); // may be NULL
        
          result = TRI_CompareValuesJson(lhsValue, rhsValue);
          if (result != 0) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
            return result;
          }
        }

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
      }

      return 0;
    }

    default:
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if two json values are the same
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckSameValueJson (const TRI_json_t* const lhs, 
                             const TRI_json_t* const rhs) {
  return (TRI_CompareValuesJson(lhs, rhs) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a json value is contained in a json list
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckInListJson (const TRI_json_t* const search, 
                          const TRI_json_t* const list) {
  size_t n;
  size_t i;

  assert(search);
  assert(list);
  assert(list->_type == TRI_JSON_LIST);
 
  // iterate over list
  n = list->_value._objects._length;
  for (i = 0; i < n; ++i) {
    TRI_json_t* listValue = (TRI_json_t*) TRI_AtVector(&list->_value._objects, i);

    if (TRI_CheckSameValueJson(search, listValue)) {
      // value is contained in list, exit
      return true;
    }
  }

  // finished list iteration, value not contained
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the elements of a list that are between the specified bounds
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_BetweenListJson (const TRI_json_t* const list,
                                 const TRI_json_t* const lower,
                                 const bool includeLower,
                                 const TRI_json_t* const upper,
                                 const bool includeUpper) {
  TRI_json_t* result;
  size_t i, n;

  assert(list);
  assert(list->_type == TRI_JSON_LIST);
  assert(lower || upper);
  
  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }

  n = list->_value._objects._length;
  for (i = 0; i < n; ++i) {
    TRI_json_t* p = TRI_AtVector(&list->_value._objects, i);

    if (lower) {
      // lower bound is set
      int compareResult = TRI_CompareValuesJson(lower, p);
      if (compareResult > 0 || (compareResult == 0 && !includeLower)) {
        // element is bigger than lower bound
        continue;
      }
    }

    if (upper) {
      // upper bound is set
      int compareResult = TRI_CompareValuesJson(p, upper);
      if (compareResult > 0 || (compareResult == 0 && !includeUpper)) {
        // element is smaller than upper bound
        continue;
      }
    }

    // element is between lower and upper bound

    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief uniquify a sorted json list into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UniquifyListJson (const TRI_json_t* const list) {
  TRI_json_t* last = NULL;
  TRI_json_t* result;
  size_t i, n;

  assert(list);
  assert(list->_type == TRI_JSON_LIST);
  
  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }

  n = list->_value._objects._length;
  for (i = 0; i < n; ++i) {
    TRI_json_t* p = TRI_AtVector(&list->_value._objects, i);
 
    // don't push value if it is the same as the last value
    if (!last || TRI_CompareValuesJson(p, last) > 0) {
      TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p);
      // remember last element
      last = p;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union of two sorted json lists into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UnionizeListsJson (const TRI_json_t* const list1, 
                                   const TRI_json_t* const list2,
                                   const bool unique) {
  TRI_json_t* last = NULL;
  TRI_json_t* result;
  size_t i1, i2;
  size_t n1, n2;

  assert(list1);
  assert(list1->_type == TRI_JSON_LIST);
  assert(list2);
  assert(list2->_type == TRI_JSON_LIST);
  
  n1 = list1->_value._objects._length;
  n2 = list2->_value._objects._length;

  // special cases for empty lists
  if (n1 == 0 && !unique) {
    return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) list2);
  }

  if (n2 == 0 && !unique) {
    return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) list1);
  }

  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }

  // reset positions
  i1 = 0;
  i2 = 0;

  // iterate over lists
  while (true) {
    // pointers to elements in both lists
    TRI_json_t* p1;
    TRI_json_t* p2;

    if (i1 < n1 && i2 < n2) {
      int compareResult;

      // both lists not yet exhausted
      p1 = TRI_AtVector(&list1->_value._objects, i1);
      p2 = TRI_AtVector(&list2->_value._objects, i2);
      compareResult = TRI_CompareValuesJson(p1, p2);

      if (compareResult < 0) {
        // left element is smaller
        if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
          last = p1;
        }
        ++i1;
      } 
      else if (compareResult > 0) {
        // right element is smaller
        if (!unique || !last || TRI_CompareValuesJson(p2, last) > 0) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
          last = p2;
        }
        ++i2;
      }
      else {
        // both elements are equal
        if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
          last = p1;
          if (!unique) {
            TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
          }
        }
        ++i1;
        ++i2;
      }
    }
    else if (i1 < n1 && i2 >= n2) {
      // only right list is exhausted
      p1 = TRI_AtVector(&list1->_value._objects, i1);

      if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
        TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
        last = p1;
      }
      ++i1;
    }
    else if (i1 >= n1 && i2 < n2) {
      // only left list is exhausted
      p2 = TRI_AtVector(&list2->_value._objects, i2);

      if (!unique || !last || TRI_CompareValuesJson(p2, last) > 0) {
        TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
        last = p2;
      }
      ++i2;
    }
    else {
      // both lists exhausted, stop!
      break;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the intersection of two sorted json lists into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_IntersectListsJson (const TRI_json_t* const list1, 
                                    const TRI_json_t* const list2,
                                    const bool unique) {
  TRI_json_t* last = NULL;
  TRI_json_t* result;
  size_t i1, i2;
  size_t n1, n2;

  assert(list1);
  assert(list1->_type == TRI_JSON_LIST);
  assert(list2);
  assert(list2->_type == TRI_JSON_LIST);
  
  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }
  
  n1 = list1->_value._objects._length;
  n2 = list2->_value._objects._length;

  // special case for empty lists
  if (n1 == 0 || n2 == 0) {
    return result; 
  }

  // reset positions
  i1 = 0;
  i2 = 0;

  // iterate over lists
  while (i1 < n1 && i2 < n2) {
    // pointers to elements in both lists
    TRI_json_t* p1 = TRI_AtVector(&list1->_value._objects, i1);
    TRI_json_t* p2 = TRI_AtVector(&list2->_value._objects, i2);

    int compareResult = TRI_CompareValuesJson(p1, p2);

    if (compareResult < 0) {
      // left element is smaller
      ++i1;
    }
    else if (compareResult > 0) {
      // right element is smaller
      ++i2;
    }
    else {
      // both elements are equal
      if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
        TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
        last = p1;
        if (!unique) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
        }
      }
      ++i1;
      ++i2;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a json list in place
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_SortListJson (TRI_json_t* const list) {
  size_t n;

  assert(list);
  assert(list->_type == TRI_JSON_LIST);

  n = list->_value._objects._length;
  if (n > 1) {
    // only sort if more than one value in list
    SortListJson((TRI_json_t*) TRI_BeginVector(&list->_value._objects), 
                 (TRI_json_t*) TRI_EndVector(&list->_value._objects));
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a JSON struct has duplicate attribute names
////////////////////////////////////////////////////////////////////////////////

bool TRI_HasDuplicateKeyJson (const TRI_json_t* const object) {
  if (object && object->_type == TRI_JSON_ARRAY) {
    const size_t n = object->_value._objects._length;
    const bool hasMultipleElements = (n > 2);

    // if we don't have attributes, we do not need to check for duplicates
    // if we only have one attribute, we don't need to check for duplicates in 
    // the array, but we need to recursively validate the array values (if 
    // array value itself is an array)
    if (n > 0) {
      TRI_associative_pointer_t hash;
      size_t i;

      if (hasMultipleElements) {
        TRI_InitAssociativePointer(&hash, 
          TRI_UNKNOWN_MEM_ZONE, 
          &TRI_HashStringKeyAssociativePointer,
          &TRI_HashStringKeyAssociativePointer,
          &TRI_EqualStringKeyAssociativePointer,
          0);
      }

      for (i = 0;  i < n; i += 2) {
        TRI_json_t* key;
        TRI_json_t* value;

        key = TRI_AtVector(&object->_value._objects, i);

        if (key->_type != TRI_JSON_STRING) {
          continue;
        }

        value = TRI_AtVector(&object->_value._objects, i + 1);

        // recursively check sub-array elements
        if (value->_type == TRI_JSON_ARRAY && TRI_HasDuplicateKeyJson(value)) {
          // duplicate found in sub-array
          if (hasMultipleElements) {
            TRI_DestroyAssociativePointer(&hash);
          }

          return true;
        }

        if (hasMultipleElements) {
          void* previous = TRI_InsertKeyAssociativePointer(&hash, key->_value._string.data, key->_value._string.data, false);
          if (previous != NULL) {
            // duplicate found
            TRI_DestroyAssociativePointer(&hash);

            return true;
          }
        }
      }

      if (hasMultipleElements) {
        TRI_DestroyAssociativePointer(&hash);
      }
    }
  }

  // no duplicate found
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two JSON documents into one
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_MergeJson (TRI_memory_zone_t* zone, 
                           const TRI_json_t* const lhs, 
                           const TRI_json_t* const rhs,
                           const bool nullMeansRemove) {
  TRI_json_t* result;

  assert(lhs->_type == TRI_JSON_ARRAY);
  assert(rhs->_type == TRI_JSON_ARRAY);

  result = MergeRecursive(zone, lhs, rhs, nullMeansRemove);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
