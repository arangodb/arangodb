////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-conv.h"

#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/logging.h"
#include "Basics/string-buffer.h"
#include "Basics/tri-strings.h"
#include "ShapedJson/shaped-json.h"

#include "V8/v8-json.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens::basics;

// #define DEBUG_JSON_SHAPER 1

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static int FillShapeValueJson (v8::Isolate* isolate,
                               TRI_shaper_t* shaper,
                               TRI_shape_value_t* dst,
                               v8::Handle<v8::Value> const json,
                               size_t level,
                               set<int>& seenHashes,
                               vector<v8::Handle<v8::Object>>& seenObjects,
                               bool create);

static v8::Handle<v8::Value> JsonShapeData (v8::Isolate* isolate,
                                            v8::Handle<v8::Value>&,
                                            TRI_shaper_t*,
                                            TRI_shape_t const*,
                                            char const*,
                                            size_t);

static v8::Handle<v8::Value> JsonShapeData (v8::Isolate* isolate,
                                            TRI_shaper_t*,
                                            TRI_shape_t const*,
                                            char const*,
                                            size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shape cache (caches pointer to last shape)
////////////////////////////////////////////////////////////////////////////////

typedef struct shape_cache_s {
  TRI_shape_sid_t    _sid;
  TRI_shape_t const* _shape;
}
shape_cache_t;

// -----------------------------------------------------------------------------
// --SECTION--                                              CONVERSION FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a null into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueNull (TRI_shaper_t* shaper,
                                TRI_shape_value_t* dst) {
  dst->_type = TRI_SHAPE_NULL;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_NULL);
  dst->_fixedSized = true;
  dst->_size = 0;
  dst->_value = nullptr;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a boolean into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueBoolean (TRI_shaper_t* shaper,
                                  TRI_shape_value_t* dst,
                                  v8::Handle<v8::Boolean> const json) {
  TRI_shape_boolean_t* ptr;

  dst->_type = TRI_SHAPE_BOOLEAN;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_BOOLEAN);
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_boolean_t);
  dst->_value = (char*) (ptr = (TRI_shape_boolean_t*) TRI_Allocate(shaper->_memoryZone, dst->_size, false));

  if (dst->_value == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  *ptr = json->Value() ? 1 : 0;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a boolean into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueBoolean (TRI_shaper_t* shaper,
                                  TRI_shape_value_t* dst,
                                  v8::Handle<v8::BooleanObject> const json) {
  TRI_shape_boolean_t* ptr;

  dst->_type = TRI_SHAPE_BOOLEAN;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_BOOLEAN);
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_boolean_t);
  dst->_value = (char*) (ptr = (TRI_shape_boolean_t*) TRI_Allocate(shaper->_memoryZone, dst->_size, false));

  if (dst->_value == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  *ptr = json->BooleanValue() ? 1 : 0;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a number into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueNumber (TRI_shaper_t* shaper,
                                 TRI_shape_value_t* dst,
                                 v8::Handle<v8::Number> const json) {
  TRI_shape_number_t* ptr;

  dst->_type = TRI_SHAPE_NUMBER;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER);
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_number_t);
  dst->_value = (char*) (ptr = (TRI_shape_number_t*) TRI_Allocate(shaper->_memoryZone, dst->_size, false));

  if (dst->_value == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  *ptr = json->Value();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a number into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueNumber (TRI_shaper_t* shaper,
                                 TRI_shape_value_t* dst,
                                 v8::Handle<v8::NumberObject> const json) {
  TRI_shape_number_t* ptr;

  dst->_type = TRI_SHAPE_NUMBER;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER);
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_number_t);
  dst->_value = (char*) (ptr = (TRI_shape_number_t*) TRI_Allocate(shaper->_memoryZone, dst->_size, false));

  if (dst->_value == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  *ptr = json->NumberValue();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a string into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueString (TRI_shaper_t* shaper,
                                 TRI_shape_value_t* dst,
                                 v8::Handle<v8::String> const json) {
  char* ptr;

  TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, json);

  if (*str == 0) {
    // empty string
    dst->_type = TRI_SHAPE_SHORT_STRING;
    dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_SHORT_STRING);
    dst->_fixedSized = true;
    dst->_size = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
    dst->_value = (ptr = (char*) TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    * ((TRI_shape_length_short_string_t*) ptr) = 1;
  }
  else {
    size_t const size = str.length();

    if (size < TRI_SHAPE_SHORT_STRING_CUT) { // includes '\0'
      dst->_type = TRI_SHAPE_SHORT_STRING;
      dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_SHORT_STRING);
      dst->_fixedSized = true;
      dst->_size = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
      dst->_value = (ptr = (char*) TRI_Allocate(shaper->_memoryZone, dst->_size, true));

      if (dst->_value == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      * ((TRI_shape_length_short_string_t*) ptr) = (TRI_shape_length_short_string_t) size + 1;
      memcpy(ptr + sizeof(TRI_shape_length_short_string_t), *str, size + 1);
    }
    else {
      dst->_type = TRI_SHAPE_LONG_STRING;
      dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_LONG_STRING);
      dst->_fixedSized = false;
      dst->_size = sizeof(TRI_shape_length_long_string_t) + size + 1;
      dst->_value = (ptr = (char*) TRI_Allocate(shaper->_memoryZone, dst->_size, false));

      if (dst->_value == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      * ((TRI_shape_length_long_string_t*) ptr) = (TRI_shape_length_long_string_t) size + 1;
      memcpy(ptr + sizeof(TRI_shape_length_long_string_t), *str, size + 1);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json list into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueList (v8::Isolate* isolate,
                               TRI_shaper_t* shaper,
                               TRI_shape_value_t* dst,
                               v8::Handle<v8::Array> const json,
                               size_t level,
                               set<int>& seenHashes,
                               vector<v8::Handle<v8::Object>>& seenObjects,
                               bool create) {
  size_t total;

  TRI_shape_value_t* values;
  TRI_shape_value_t* p;
  TRI_shape_value_t* e;

  bool hs;
  bool hl;

  TRI_shape_sid_t s;
  TRI_shape_sid_t l;

  TRI_shape_size_t* offsets;
  TRI_shape_size_t offset;

  TRI_shape_t const* found;

  char* ptr;

  // check for special case "empty list"
  uint32_t n = json->Length();

  if (n == 0) {
    dst->_type = TRI_SHAPE_LIST;
    dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_LIST);

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t);
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, false)));

    if (dst->_value == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    * (TRI_shape_length_list_t*) ptr = 0;

    return TRI_ERROR_NO_ERROR;
  }

  // convert into TRI_shape_value_t array
  p = (values = static_cast<TRI_shape_value_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shape_value_t) * n, true)));

  if (p == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  total = 0;
  e = values + n;

  for (uint32_t i = 0;  i < n;  ++i, ++p) {
    v8::Handle<v8::Value> el = json->Get(i);
    int res = FillShapeValueJson(isolate, shaper, p, el, level + 1, seenHashes, seenObjects, create);

    if (res != TRI_ERROR_NO_ERROR) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      return res;
    }

    total += p->_size;
  }

  // check if this list is homoegenous
  hs = true;
  hl = true;

  s = values[0]._sid;
  l = values[0]._size;
  p = values;

  for (;  p < e;  ++p) {
    if (p->_sid != s) {
      hs = false;
      break;
    }

    if (p->_size != l) {
      hl = false;
    }
  }

  // homogeneous sized
  if (hs && hl) {
    auto shape = static_cast<TRI_homogeneous_sized_list_shape_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_homogeneous_sized_list_shape_t), true));

    if (shape == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    shape->base._size = sizeof(TRI_homogeneous_sized_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_SIZED_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;
    shape->_sizeEntry = l;

    found = shaper->findShape(shaper, &shape->base, create);

    if (found == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      LOG_TRACE("shaper failed to find shape of type %d", (int) shape->base._type);

      if (! create) {
        return TRI_RESULT_ELEMENT_NOT_FOUND;
      }

      return TRI_ERROR_INTERNAL;
    }

    TRI_ASSERT(found != nullptr);

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t) + total;
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, false)));

    if (dst->_value == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = (TRI_shape_length_list_t) n;
    ptr += sizeof(TRI_shape_length_list_t);

    for (p = values;  p < e;  ++p) {
      memcpy(ptr, p->_value, p->_size);
      ptr += p->_size;
    }
  }

  // homogeneous
  else if (hs) {
    auto shape = static_cast<TRI_homogeneous_list_shape_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_homogeneous_list_shape_t), true));

    if (shape == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    shape->base._size = sizeof(TRI_homogeneous_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;

    // if found returns non-NULL, it will free the shape!!
    found = shaper->findShape(shaper, &shape->base, create);

    if (found == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      LOG_TRACE("shaper failed to find shape %d", (int) shape->base._type);

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      if (! create) {
        return TRI_RESULT_ELEMENT_NOT_FOUND;
      }

      return TRI_ERROR_INTERNAL;
    }

    TRI_ASSERT(found != nullptr);

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    offset = sizeof(TRI_shape_length_list_t) + (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, true)));

    if (dst->_value == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = (TRI_shape_length_list_t) n;
    ptr += sizeof(TRI_shape_length_list_t);

    offsets = (TRI_shape_size_t*) ptr;
    ptr += (n + 1) * sizeof(TRI_shape_size_t);

    for (p = values;  p < e;  ++p) {
      *offsets++ = offset;
      offset += p->_size;

      memcpy(ptr, p->_value, p->_size);
      ptr += p->_size;
    }

    *offsets = offset;
  }

  // in-homogeneous
  else {
    dst->_type = TRI_SHAPE_LIST;
    dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_LIST);

    offset =
      sizeof(TRI_shape_length_list_t)
      + n * sizeof(TRI_shape_sid_t)
      + (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = (char*) TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = (TRI_shape_length_list_t) n;
    ptr += sizeof(TRI_shape_length_list_t);

    TRI_shape_sid_t* sids = (TRI_shape_sid_t*) ptr;
    ptr += n * sizeof(TRI_shape_sid_t);

    offsets = (TRI_shape_size_t*) ptr;
    ptr += (n + 1) * sizeof(TRI_shape_size_t);

    for (p = values;  p < e;  ++p) {
      *sids++ = p->_sid;

      *offsets++ = offset;
      offset += p->_size;

      memcpy(ptr, p->_value, p->_size);
      ptr += p->_size;
    }

    *offsets = offset;
  }

  // free TRI_shape_value_t array
  for (p = values;  p < e;  ++p) {
    if (p->_value != nullptr) {
      TRI_Free(shaper->_memoryZone, p->_value);
    }
  }

  TRI_Free(shaper->_memoryZone, values);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json array into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueArray (v8::Isolate* isolate,
                                TRI_shaper_t* shaper,
                                TRI_shape_value_t* dst,
                                v8::Handle<v8::Object> const json,
                                size_t level,
                                set<int>& seenHashes,
                                vector<v8::Handle<v8::Object>>& seenObjects,
                                bool create) {
  v8::HandleScope scope(isolate);
  TRI_shape_value_t* values;
  TRI_shape_value_t* p;
  TRI_shape_value_t* e;

  TRI_array_shape_t* a;

  TRI_shape_sid_t* sids;
  TRI_shape_aid_t* aids;
  TRI_shape_size_t* offsetsF;
  TRI_shape_size_t* offsetsV;
  TRI_shape_size_t offset;

  TRI_shape_t const* found;

  char* ptr;

  // number of attributes
  v8::Handle<v8::Array> names = json->GetOwnPropertyNames();
  uint32_t n = names->Length();

  // convert into TRI_shape_value_t array
  p = (values = static_cast<TRI_shape_value_t*>(TRI_Allocate(shaper->_memoryZone, n * sizeof(TRI_shape_value_t), true)));

  if (p == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  size_t total = 0;

  size_t f = 0;
  size_t v = 0;

  for (uint32_t i = 0;  i < n;  ++i, ++p) {
    v8::Handle<v8::Value> key = names->Get(i);

    // first find an identifier for the name
    TRI_Utf8ValueNFC keyStr(TRI_UNKNOWN_MEM_ZONE, key);

    if (*keyStr == 0 || keyStr.length() == 0) {
      --p;
      continue;
    }

    if ((*keyStr)[0] == '_' && level == 0) {
      // on top level, strip reserved attributes before shaping
      char const* k = (*keyStr);
      if (strcmp(k, "_key") == 0 || 
          strcmp(k, "_rev") == 0 ||
          strcmp(k, "_id") == 0 ||
          strcmp(k, "_from") == 0 ||
          strcmp(k, "_to") == 0) {
        // found a reserved attribute - discard it
        --p;
        continue;
      }
    }

    if (create) {
      p->_aid = shaper->findOrCreateAttributeByName(shaper, *keyStr);
    }
    else {
      p->_aid = shaper->lookupAttributeByName(shaper, *keyStr);
    }

    int res;

    // convert value
    if (p->_aid == 0) {
      if (create) {
        res = TRI_ERROR_INTERNAL;
      }
      else {
        res = TRI_RESULT_ELEMENT_NOT_FOUND;
      }
    }
    else {
      v8::Handle<v8::Value> val = json->Get(key);
      res = FillShapeValueJson(isolate, shaper, p, val, level + 1, seenHashes, seenObjects, create);
    }

    if (res != TRI_ERROR_NO_ERROR) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      return res;
    }

    total += p->_size;

    // count fixed and variable sized values
    if (p->_fixedSized) {
      ++f;
    }
    else {
      ++v;
    }
  }

  // adjust n
  n = (uint32_t) (f + v);

  // add variable offset table size
  total += (v + 1) * sizeof(TRI_shape_size_t);

  // now sort the shape entries
  if (n > 1) {
    TRI_SortShapeValues(values, (size_t) n);
  }

#ifdef DEBUG_JSON_SHAPER
  printf("shape values\n------------\ntotal: %u, fixed: %u, variable: %u\n",
         (unsigned int) n,
         (unsigned int) f,
         (unsigned int) v);
  TRI_PrintShapeValues(values, n);
  printf("\n");
#endif

  // generate shape structure
  size_t const totalSize =
    sizeof(TRI_array_shape_t)
    + n * sizeof(TRI_shape_sid_t)
    + n * sizeof(TRI_shape_aid_t)
    + (f + 1) * sizeof(TRI_shape_size_t);

  a = (TRI_array_shape_t*) (ptr = (char*) TRI_Allocate(shaper->_memoryZone, totalSize, true));

  if (ptr == nullptr) {
    e = values + n;

    for (p = values;  p < e;  ++p) {
      if (p->_value != nullptr) {
        TRI_Free(shaper->_memoryZone, p->_value);
      }
    }

    TRI_Free(shaper->_memoryZone, values);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  a->base._type = TRI_SHAPE_ARRAY;
  a->base._size = (TRI_shape_size_t) totalSize;
  a->base._dataSize = (v == 0) ? total : TRI_SHAPE_SIZE_VARIABLE;

  a->_fixedEntries = f;
  a->_variableEntries = v;

  ptr += sizeof(TRI_array_shape_t);

  // array of shape identifiers
  sids = (TRI_shape_sid_t*) ptr;
  ptr += n * sizeof(TRI_shape_sid_t);

  // array of attribute identifiers
  aids = (TRI_shape_aid_t*) ptr;
  ptr += n * sizeof(TRI_shape_aid_t);

  // array of offsets for fixed part (within the shape)
  offset = (v + 1) * sizeof(TRI_shape_size_t);
  offsetsF = (TRI_shape_size_t*) ptr;

  // fill destination (except sid)
  dst->_type = TRI_SHAPE_ARRAY;

  dst->_fixedSized = true;
  dst->_size = total;
  dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, true)));

  if (ptr == nullptr) {
    e = values + n;

    for (p = values;  p < e;  ++p) {
      if (p->_value != nullptr) {
        TRI_Free(shaper->_memoryZone, p->_value);
      }
    }

    TRI_Free(shaper->_memoryZone, values);
    TRI_Free(shaper->_memoryZone, a);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // array of offsets for variable part (within the value)
  offsetsV = (TRI_shape_size_t*) ptr;
  ptr += (v + 1) * sizeof(TRI_shape_size_t);

  // and fill in attributes
  e = values + n;

  for (p = values;  p < e;  ++p) {
    *aids++ = p->_aid;
    *sids++ = p->_sid;

    memcpy(ptr, p->_value, p->_size);
    ptr += p->_size;

    dst->_fixedSized &= p->_fixedSized;

    if (p->_fixedSized) {
      *offsetsF++ = offset;
      offset += p->_size;
      *offsetsF = offset;
    }
    else {
      *offsetsV++ = offset;
      offset += p->_size;
      *offsetsV = offset;
    }
  }

  // free TRI_shape_value_t array
  for (p = values;  p < e;  ++p) {
    if (p->_value != nullptr) {
      TRI_Free(shaper->_memoryZone, p->_value);
    }
  }

  TRI_Free(shaper->_memoryZone, values);

  // lookup this shape
  found = shaper->findShape(shaper, &a->base, create);

  if (found == nullptr) {
    LOG_TRACE("shaper failed to find shape %d", (int) a->base._type);
    TRI_Free(shaper->_memoryZone, a);

    if (! create) {
      return TRI_RESULT_ELEMENT_NOT_FOUND;
    }

    return TRI_ERROR_INTERNAL;
  }

  // and finally add the sid
  dst->_sid = found->_sid;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static int FillShapeValueJson (v8::Isolate* isolate,
                               TRI_shaper_t* shaper,
                               TRI_shape_value_t* dst,
                               v8::Handle<v8::Value> const json,
                               size_t level,
                               set<int>& seenHashes,
                               vector<v8::Handle<v8::Object>>& seenObjects,
                               bool create) {
  v8::HandleScope scope(isolate);
  if (json->IsRegExp() || json->IsFunction() || json->IsExternal()) {
    LOG_TRACE("shaper failed because regexp/function/external/date object cannot be converted");
    return TRI_ERROR_BAD_PARAMETER;
  }
  
  if (json->IsObject() && ! json->IsArray()) {
    v8::Handle<v8::Object> o = json->ToObject();
    v8::Handle<v8::String> toJsonString = TRI_V8_ASCII_STRING("toJSON");
    if (o->Has(toJsonString)) {
      v8::Handle<v8::Value> func = o->Get(toJsonString);
      if (func->IsFunction()) {
        v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(func);

        v8::Handle<v8::Value> args;
        v8::Handle<v8::Value> result = toJson->Call(o, 0, &args);

        if (! result.IsEmpty()) {
          return FillShapeValueString(shaper, dst, result->ToString());
        }
      }
    }

    // fall-through intentional
  
    // check for cycles
    int hash = o->GetIdentityHash();

    if (seenHashes.find(hash) != seenHashes.end()) {
      // LOG_TRACE("found hash %d", hash);

      for (auto it = seenObjects.begin();  it != seenObjects.end();  ++it) {
        if (json->StrictEquals(*it)) {
          LOG_TRACE("found duplicate for hash %d", hash);
          return TRI_ERROR_ARANGO_SHAPER_FAILED;
        }
      }
    }
    else {
      seenHashes.insert(hash);
    }

    seenObjects.push_back(o);
  }

  if (json->IsNull() || json->IsUndefined()) {
    return FillShapeValueNull(shaper, dst);
  }

  else if (json->IsBoolean()) {
    return FillShapeValueBoolean(shaper, dst, json->ToBoolean());
  }

  else if (json->IsBooleanObject()) {
    return FillShapeValueBoolean(shaper, dst, v8::Handle<v8::BooleanObject>::Cast(json));
  }

  else if (json->IsNumber()) {
    return FillShapeValueNumber(shaper, dst, json->ToNumber());
  }

  else if (json->IsNumberObject()) {
    return FillShapeValueNumber(shaper, dst, v8::Handle<v8::NumberObject>::Cast(json));
  }

  else if (json->IsString()) {
    return FillShapeValueString(shaper, dst, json->ToString());
  }

  else if (json->IsStringObject()) {
    return FillShapeValueString(shaper, dst, v8::Handle<v8::StringObject>::Cast(json)->ValueOf());
  }

  else if (json->IsArray()) {
    return FillShapeValueList(isolate, shaper, dst, v8::Handle<v8::Array>::Cast(json), level, seenHashes, seenObjects, create);
  }

  else if (json->IsObject()) {
    int res = FillShapeValueArray(isolate, shaper, dst, json->ToObject(), level, seenHashes, seenObjects, create);
    seenObjects.pop_back();
    // cannot remove hash value from seenHashes because multiple objects might have the same
    // hash values (collisions)
    return res;
  }

  LOG_TRACE("shaper failed to convert object");
  return TRI_ERROR_BAD_PARAMETER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data null blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataNull (v8::Isolate* isolate,
                                                TRI_shaper_t* shaper,
                                                TRI_shape_t const* shape,
                                                char const* data,
                                                size_t size) {
  return v8::Null(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data boolean blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataBoolean (v8::Isolate* isolate,
                                                   TRI_shaper_t* shaper,
                                                   TRI_shape_t const* shape,
                                                   char const* data,
                                                   size_t size) {
  bool v;

  v = (* (TRI_shape_boolean_t const*) data) != 0;

  return v8::Boolean::New(isolate, v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data number blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataNumber (v8::Isolate* isolate,
                                                  TRI_shaper_t* shaper,
                                                  TRI_shape_t const* shape,
                                                  char const* data,
                                                  size_t size) {
  TRI_shape_number_t v;

  v = * (TRI_shape_number_t const*) data;

  return v8::Number::New(isolate, v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data short string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataShortString (v8::Isolate* isolate,
                                                       TRI_shaper_t* shaper,
                                                       TRI_shape_t const* shape,
                                                       char const* data,
                                                       size_t size) {
  TRI_shape_length_short_string_t l;

  l = * (TRI_shape_length_short_string_t const*) data;
  data += sizeof(TRI_shape_length_short_string_t);

  return TRI_V8_PAIR_STRING(data, l - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data long string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataLongString (v8::Isolate* isolate,
                                                      TRI_shaper_t* shaper,
                                                      TRI_shape_t const* shape,
                                                      char const* data,
                                                      size_t size) {
  TRI_shape_length_long_string_t l;

  l = * (TRI_shape_length_long_string_t const*) data;
  data += sizeof(TRI_shape_length_long_string_t);

  return TRI_V8_PAIR_STRING(data, l - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merges a data array blob into an existing json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataArray (v8::Isolate* isolate,
                                                 v8::Handle<v8::Value>& value,
                                                 TRI_shaper_t* shaper,
                                                 TRI_shape_t const* shape,
                                                 char const* data,
                                                 size_t size) {
  v8::EscapableHandleScope scope(isolate);
  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsetsF;
  TRI_shape_size_t const* offsetsV;
  TRI_shape_size_t f;
  TRI_shape_size_t i;
  TRI_shape_size_t n;
  TRI_shape_size_t v;
  shape_cache_t shapeCache;
  char const* qtr;

  v8::Handle<v8::Object> array = v8::Handle<v8::Object>::Cast(value);

  s = (TRI_array_shape_t const*) shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;

  qtr = (char const*) shape;

  qtr += sizeof(TRI_array_shape_t);

  sids = (TRI_shape_sid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_sid_t);

  aids = (TRI_shape_aid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_aid_t);

  offsetsF = (TRI_shape_size_t const*) qtr;
  shapeCache._sid   = 0;
  shapeCache._shape = 0;

  for (i = 0;  i < f;  ++i, ++sids, ++aids, ++offsetsF) {
    TRI_shape_sid_t sid = *sids;

    TRI_shape_t const* subshape;
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    }
    else {
      shapeCache._shape = subshape = shaper->lookupShapeId(shaper, sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_shape_aid_t aid = *aids;
    char const* name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    const TRI_shape_size_t offset = *offsetsF;
    v8::Handle<v8::Value> element = JsonShapeData(isolate, shaper, subshape, data + offset, offsetsF[1] - offset);
    array->Set(TRI_V8_STRING(name), element);
  }

  offsetsV = (TRI_shape_size_t const*) data;

  for (i = 0;  i < v;  ++i, ++sids, ++aids, ++offsetsV) {
    TRI_shape_sid_t sid = *sids;

    TRI_shape_t const* subshape;
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    }
    else {
      shapeCache._shape = subshape = shaper->lookupShapeId(shaper, sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_shape_aid_t aid = *aids;
    char const* name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    const TRI_shape_size_t offset = *offsetsV;
    v8::Handle<v8::Value> element = JsonShapeData(isolate, shaper, subshape, data + offset, offsetsV[1] - offset);
    array->Set(TRI_V8_STRING(name), element);
  }

  return scope.Escape<v8::Value>(array);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data array blob into a new json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataArray (v8::Isolate* isolate,
                                                 TRI_shaper_t* shaper,
                                                 TRI_shape_t const* shape,
                                                 char const* data,
                                                 size_t size) {
  v8::EscapableHandleScope scope(isolate);
  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsetsF;
  TRI_shape_size_t const* offsetsV;
  TRI_shape_size_t f;
  TRI_shape_size_t i;
  TRI_shape_size_t n;
  TRI_shape_size_t v;
  shape_cache_t shapeCache;
  char const* qtr;

  v8::Handle<v8::Object> array;

  s = (TRI_array_shape_t const*) shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;

  qtr = (char const*) shape;
  array = v8::Object::New(isolate);

  qtr += sizeof(TRI_array_shape_t);

  sids = (TRI_shape_sid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_sid_t);

  aids = (TRI_shape_aid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_aid_t);

  offsetsF = (TRI_shape_size_t const*) qtr;
  shapeCache._sid   = 0;
  shapeCache._shape = 0;

  for (i = 0;  i < f;  ++i, ++sids, ++aids, ++offsetsF) {
    TRI_shape_sid_t sid = *sids;

    TRI_shape_t const* subshape;
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    }
    else {
      shapeCache._shape = subshape = shaper->lookupShapeId(shaper, sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_shape_aid_t aid = *aids;
    char const* name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    const TRI_shape_size_t offset = *offsetsF;
    v8::Handle<v8::Value> element = JsonShapeData(isolate, shaper, subshape, data + offset, offsetsF[1] - offset);
    array->Set(TRI_V8_STRING(name), element);
  }

  offsetsV = (TRI_shape_size_t const*) data;

  for (i = 0;  i < v;  ++i, ++sids, ++aids, ++offsetsV) {
    TRI_shape_sid_t sid = *sids;

    TRI_shape_t const* subshape;
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    }
    else {
      shapeCache._shape = subshape = shaper->lookupShapeId(shaper, sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_shape_aid_t aid = *aids;
    char const* name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    const TRI_shape_size_t offset = *offsetsV;
    v8::Handle<v8::Value> element = JsonShapeData(isolate, shaper, subshape, data + offset, offsetsV[1] - offset);
    array->Set(TRI_V8_STRING(name), element);
  }

  return scope.Escape<v8::Value>(array);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataList (v8::Isolate* isolate,
                                                TRI_shaper_t* shaper,
                                                TRI_shape_t const* shape,
                                                char const* data,
                                                size_t size) {
  v8::EscapableHandleScope scope(isolate);
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsets;
  shape_cache_t shapeCache;
  char const* ptr;

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  if (l == 0) {
    return scope.Escape<v8::Value>(v8::Array::New(isolate, 0));
  }

  shapeCache._sid   = 0;
  shapeCache._shape = 0;
  v8::Handle<v8::Array> list = v8::Array::New(isolate, l);

  ptr += sizeof(TRI_shape_length_list_t);
  sids = (TRI_shape_sid_t const*) ptr;

  ptr += l * sizeof(TRI_shape_sid_t);
  offsets = (TRI_shape_size_t const*) ptr;

  for (i = 0;  i < l;  ++i, ++sids, ++offsets) {
    TRI_shape_sid_t sid = *sids;

    TRI_shape_t const* subshape;
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    }
    else {
      shapeCache._shape = subshape = shaper->lookupShapeId(shaper, sid);
      shapeCache._sid   = sid;
    }

    if (subshape == nullptr) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    const TRI_shape_size_t offset = *offsets;
    v8::Handle<v8::Value> element = JsonShapeData(isolate, shaper, subshape, data + offset, offsets[1] - offset);
    list->Set((uint32_t) i, element);
  }

  return scope.Escape<v8::Value>(list);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataHomogeneousList (v8::Isolate* isolate,
                                                           TRI_shaper_t* shaper,
                                                           TRI_shape_t const* shape,
                                                           char const* data,
                                                           size_t size) {
  v8::EscapableHandleScope scope(isolate);
  TRI_homogeneous_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t const* offsets;
  char const* ptr;
  TRI_shape_t const* subshape;

  s = (TRI_homogeneous_list_shape_t const*) shape;
  sid = s->_sidEntry;

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  offsets = (TRI_shape_size_t const*) ptr;

  subshape = shaper->lookupShapeId(shaper, sid);

  if (subshape == nullptr) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
    return scope.Escape<v8::Value>(v8::Array::New(isolate));
  }

  v8::Handle<v8::Array> list = v8::Array::New(isolate, l);

  for (i = 0;  i < l;  ++i, ++offsets) {
    TRI_shape_size_t offset = *offsets;
    v8::Handle<v8::Value> element = JsonShapeData(isolate, shaper, subshape, data + offset, offsets[1] - offset);

    list->Set((uint32_t) i, element);
  }

  return scope.Escape<v8::Value>(list);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous sized list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataHomogeneousSizedList (v8::Isolate* isolate,
                                                                TRI_shaper_t* shaper,
                                                                TRI_shape_t const* shape,
                                                                char const* data,
                                                                size_t size) {
  v8::EscapableHandleScope scope(isolate);
  TRI_homogeneous_sized_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t length;
  char const* ptr;
  TRI_shape_t const* subshape;

  s = (TRI_homogeneous_sized_list_shape_t const*) shape;

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  if (l == 0) {
    return scope.Escape<v8::Value>(v8::Array::New(isolate));
  }

  sid    = s->_sidEntry;
  length = s->_sizeEntry;

  subshape = shaper->lookupShapeId(shaper, sid);

  if (subshape == nullptr) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
    return scope.Escape<v8::Value>(v8::Array::New(isolate));
  }

  TRI_shape_size_t offset = sizeof(TRI_shape_length_list_t);
 
  v8::Handle<v8::Array> list = v8::Array::New(isolate, l);

  for (i = 0;  i < l;  ++i, offset += length) {
    v8::Handle<v8::Value> element = JsonShapeData(isolate, shaper, subshape, data + offset, length);
    list->Set((uint32_t) i, element);
  }

  return scope.Escape<v8::Value>(list);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merges a data blob into an existing json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeData (v8::Isolate* isolate,
                                            v8::Handle<v8::Value>& value,
                                            TRI_shaper_t* shaper,
                                            TRI_shape_t const* shape,
                                            char const* data,
                                            size_t size) {
  if (shape == nullptr) {
    v8::EscapableHandleScope scope(isolate);
    return scope.Escape<v8::Value>(v8::Null(isolate));
  }

  TRI_ASSERT(shape->_type == TRI_SHAPE_ARRAY);
  return JsonShapeDataArray(isolate, value, shaper, shape, data, size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data blob into a new json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeData (v8::Isolate* isolate,
                                            TRI_shaper_t* shaper,
                                            TRI_shape_t const* shape,
                                            char const* data,
                                            size_t size) {
  if (shape == nullptr) {
    v8::EscapableHandleScope scope(isolate);
    return scope.Escape<v8::Value>(v8::Null(isolate));
  }

  switch (shape->_type) {
    case TRI_SHAPE_NULL:
      return JsonShapeDataNull(isolate, shaper, shape, data, size);

    case TRI_SHAPE_BOOLEAN:
      return JsonShapeDataBoolean(isolate, shaper, shape, data, size);

    case TRI_SHAPE_NUMBER:
      return JsonShapeDataNumber(isolate, shaper, shape, data, size);

    case TRI_SHAPE_SHORT_STRING:
      return JsonShapeDataShortString(isolate, shaper, shape, data, size);

    case TRI_SHAPE_LONG_STRING:
      return JsonShapeDataLongString(isolate, shaper, shape, data, size);

    case TRI_SHAPE_ARRAY:
      return JsonShapeDataArray(isolate, shaper, shape, data, size);

    case TRI_SHAPE_LIST:
      return JsonShapeDataList(isolate, shaper, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_LIST:
      return JsonShapeDataHomogeneousList(isolate, shaper, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      return JsonShapeDataHomogeneousSizedList(isolate, shaper, shape, data, size);
  }
  {
    v8::EscapableHandleScope scope(isolate);
    return scope.Escape<v8::Value>(v8::Null(isolate));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NULL into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonNull (v8::Isolate* isolate, TRI_json_t const* json) {
  v8::EscapableHandleScope scope(isolate);
  return scope.Escape<v8::Value>(v8::Null(isolate)); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t BOOLEAN into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonBoolean (v8::Isolate* isolate, TRI_json_t const* json) {
  v8::EscapableHandleScope scope(isolate);
  return scope.Escape<v8::Value>(v8::Boolean::New(isolate, json->_value._boolean));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NUMBER into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonNumber (v8::Isolate* isolate, TRI_json_t const* json) {
  v8::EscapableHandleScope scope(isolate);
  return scope.Escape<v8::Value>(v8::Number::New(isolate, json->_value._number));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NUMBER into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonString (v8::Isolate* isolate, TRI_json_t const* json) {
  v8::EscapableHandleScope scope(isolate);
  return scope.Escape<v8::Value>(TRI_V8_PAIR_STRING(json->_value._string.data, json->_value._string.length - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t ARRAY into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectJsonArray (v8::Isolate* isolate, TRI_json_t const* json) {
  v8::EscapableHandleScope scope(isolate);
  v8::Handle<v8::Object> object = v8::Object::New(isolate);

  size_t const n = json->_value._objects._length;

  for (size_t i = 0;  i < n;  i += 2) {
    TRI_json_t const* key = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));

    if (! TRI_IsStringJson(key)) {
      continue;
    }

    TRI_json_t const* j = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));
    object->Set(TRI_V8_PAIR_STRING(key->_value._string.data, key->_value._string.length - 1), TRI_ObjectJson(isolate, j));
  }

  return scope.Escape<v8::Value>(object);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t LIST into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectJsonList (v8::Isolate* isolate, TRI_json_t const* json) {
  v8::EscapableHandleScope scope(isolate);
  uint32_t const n = (uint32_t) json->_value._objects._length;

  v8::Handle<v8::Array> object = v8::Array::New(isolate, (int) n);

  for (uint32_t i = 0;  i < n;  ++i) {
    TRI_json_t const* j = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
    v8::Handle<v8::Value> val = TRI_ObjectJson(isolate, j);

    object->Set(i, val);
  }

  return scope.Escape<v8::Value>(object);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts keys or values from a TRI_json_t* array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExtractArray (v8::Isolate* isolate,
                                           TRI_json_t const* json,
                                           size_t offset) {
  v8::EscapableHandleScope scope(isolate);
  if (json == nullptr || json->_type != TRI_JSON_ARRAY) {
    return scope.Escape<v8::Value>(v8::Undefined(isolate));
  }
  
  size_t const n = TRI_LengthVector(&json->_value._objects);
  
  v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n / 2));
  uint32_t count = 0;

  for (size_t i = offset; i < n; i += 2) {
    TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));

    if (value != nullptr) {
      result->Set(count++, TRI_ObjectJson(isolate, value));
    }
  }

  return scope.Escape<v8::Value>(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes the names of an associative char* array into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Array> TRI_ArrayAssociativePointer (v8::Isolate* isolate,
                                                   TRI_associative_pointer_t const* array) {
  v8::EscapableHandleScope scope(isolate);
  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  uint32_t j = 0;
  uint32_t n = (uint32_t) array->_nrAlloc;

  for (uint32_t i = 0;  i < n;  ++i) {
    char* value = (char*) array->_table[i];

    if (value == nullptr) {
      continue;
    }

    result->Set(j++, v8::String::NewFromUtf8(isolate, value));/// TODO: strlen...
  }

  return scope.Escape<v8::Array>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the keys of a TRI_json_t* array into a V8 list
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_KeysJson (v8::Isolate* isolate,
                                    TRI_json_t const* json) {
  return ExtractArray(isolate, json, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the values of a TRI_json_t* array into a V8 list
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ValuesJson (v8::Isolate* isolate,
                                      TRI_json_t const* json) {
  return ExtractArray(isolate, json, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ObjectJson (v8::Isolate* isolate,
                                      TRI_json_t const* json) {
  if (json == nullptr) {
    return v8::Undefined(isolate);
  }

  switch (json->_type) {
    case TRI_JSON_UNUSED:
      return v8::Undefined(isolate);

    case TRI_JSON_NULL:
      return ObjectJsonNull(isolate, json);

    case TRI_JSON_BOOLEAN:
      return ObjectJsonBoolean(isolate, json);

    case TRI_JSON_NUMBER:
      return ObjectJsonNumber(isolate, json);

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE:
      return ObjectJsonString(isolate, json);

    case TRI_JSON_ARRAY:
      return ObjectJsonArray(isolate, json);

    case TRI_JSON_LIST:
      return ObjectJsonList(isolate, json);
  }

  return v8::Undefined(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_shaped_json_t into an existing V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_JsonShapeData (v8::Isolate* isolate,
                                         v8::Handle<v8::Value> value,
                                         TRI_shaper_t* shaper,
                                         TRI_shape_t const* shape,
                                         char const* data,
                                         size_t size) {
  return JsonShapeData(isolate, value, shaper, shape, data, size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_shaped_json_t into a new V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_JsonShapeData (v8::Isolate* isolate,
                                         TRI_shaper_t* shaper,
                                         TRI_shape_t const* shape,
                                         char const* data,
                                         size_t size) {
  return JsonShapeData(isolate, shaper, shape, data, size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonV8Object (v8::Isolate* isolate,
                                           v8::Handle<v8::Value> const object,
                                           TRI_shaper_t* shaper,
                                           bool create) {
  TRI_shape_value_t dst;
  set<int> seenHashes;
  vector<v8::Handle<v8::Object>> seenObjects;

  int res = FillShapeValueJson(isolate, shaper, &dst, object, 0, seenHashes, seenObjects, create);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      TRI_set_errno(res);
    }
    else {
      TRI_set_errno(TRI_ERROR_ARANGO_SHAPER_FAILED);
    }
    return nullptr;
  }

  TRI_shaped_json_t* shaped = static_cast<TRI_shaped_json_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shaped_json_t), false));

  if (shaped == nullptr) {
    return nullptr;
  }

  shaped->_sid = dst._sid;
  shaped->_data.length = (uint32_t) dst._size;
  shaped->_data.data = dst._value;

  return shaped;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a TRI_shaped_json_t in place
////////////////////////////////////////////////////////////////////////////////

int TRI_FillShapedJsonV8Object (v8::Isolate* isolate,
                                v8::Handle<v8::Value> const object,
                                TRI_shaped_json_t* result,
                                TRI_shaper_t* shaper,
                                bool create) {
  TRI_shape_value_t dst;
  set<int> seenHashes;
  vector<v8::Handle<v8::Object>> seenObjects;

  int res = FillShapeValueJson(isolate, shaper, &dst, object, 0, seenHashes, seenObjects, create);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res != TRI_RESULT_ELEMENT_NOT_FOUND) {
      res = TRI_ERROR_BAD_PARAMETER;
    }
    return TRI_set_errno(res);
  }

  result->_sid = dst._sid;
  result->_data.length = (uint32_t) dst._size;
  result->_data.data = dst._value;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a TRI_json_t value
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ObjectToJson (v8::Isolate* isolate,
                                 v8::Handle<v8::Value> const parameter,
                                 set<int>& seenHashes,
                                 vector<v8::Handle<v8::Object>>& seenObjects) {
  v8::HandleScope scope(isolate);
  if (parameter->IsBoolean()) {
    v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean();
    return TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, booleanParameter->Value());
  }

  if (parameter->IsBooleanObject()) {
    v8::Handle<v8::BooleanObject> bo = v8::Handle<v8::BooleanObject>::Cast(parameter);
    return TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, bo->BooleanValue());
  }

  if (parameter->IsNull()) {
    return TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE);
  }

  if (parameter->IsNumber()) {
    v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
    return TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, numberParameter->Value());
  }
  
  if (parameter->IsNumberObject()) {
    v8::Handle<v8::NumberObject> no = v8::Handle<v8::NumberObject>::Cast(parameter);
    return TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, no->NumberValue());
  }

  if (parameter->IsString() || parameter->IsStringObject()) {
    v8::Handle<v8::String> stringParameter= parameter->ToString();
    TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, stringParameter);
    // move the string pointer into the JSON object
    if (*str != 0) {
      TRI_json_t* j = TRI_CreateString2Json(TRI_UNKNOWN_MEM_ZONE, *str, str.length());
      // this passes ownership for the utf8 string to the JSON object
      str.steal();

      // the Utf8ValueNFC dtor won't free the string now
      return j;
    }

    return nullptr;
  }
  
  if (parameter->IsArray()) {
    v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>::Cast(parameter);
    uint32_t const n = arrayParameter->Length();

    TRI_json_t* listJson = TRI_CreateList2Json(TRI_UNKNOWN_MEM_ZONE, static_cast<size_t const>(n));

    if (listJson != nullptr) {
      for (uint32_t j = 0; j < n; ++j) {
        TRI_json_t* result = ObjectToJson(isolate,
                                          arrayParameter->Get(j),
                                          seenHashes,
                                          seenObjects);

        if (result != nullptr) {
          TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, listJson, result);
        }
      }
    }
    return listJson;
  }
  
  if (parameter->IsRegExp() || 
      parameter->IsFunction() || 
      parameter->IsExternal()) { 
    return nullptr;
  }

  if (parameter->IsObject() && ! parameter->IsArray()) {
    v8::Handle<v8::Object> o = parameter->ToObject();

    // first check if the object has a "toJSON" function
    v8::Handle<v8::String> toJsonString = TRI_V8_ASCII_STRING("toJSON");
    if (o->Has(toJsonString)) {
      // call it if yes
      v8::Handle<v8::Value> func = o->Get(toJsonString);
      if (func->IsFunction()) {
        v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(func);

        v8::Handle<v8::Value> args;
        v8::Handle<v8::Value> result = toJson->Call(o, 0, &args);

        if (! result.IsEmpty()) {
          // return whatever toJSON returned

          TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, result->ToString());
          // move the string pointer into the JSON object
          if (*str != 0) {
            TRI_json_t* j = TRI_CreateString2Json(TRI_UNKNOWN_MEM_ZONE, *str, str.length());
            // this passes ownership for the utf8 string to the JSON object
            str.steal();

            // the Utf8ValueNFC dtor won't free the string now
            return j;
          }
        }
      }

      // fall-through intentional
    }

    int hash = o->GetIdentityHash();

    if (seenHashes.find(hash) != seenHashes.end()) {
      // LOG_TRACE("found hash %d", hash);

      for (auto it = seenObjects.begin();  it != seenObjects.end();  ++it) {
        if (parameter->StrictEquals(*it)) {
          LOG_TRACE("found duplicate for hash %d", hash);
          return nullptr;
        }
      }
    }
    else {
      seenHashes.insert(hash);
    }

    seenObjects.push_back(o);

    v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>::Cast(parameter);
    v8::Handle<v8::Array> names = arrayParameter->GetOwnPropertyNames();
    uint32_t const n = names->Length();

    TRI_json_t* arrayJson = TRI_CreateArray2Json(TRI_UNKNOWN_MEM_ZONE, static_cast<size_t const>(n));

    if (arrayJson != nullptr && n > 0) {
      for (uint32_t j = 0; j < n; ++j) {
        v8::Handle<v8::Value> key  = names->Get(j);
        TRI_json_t* result = ObjectToJson(isolate,
                                          arrayParameter->Get(key),
                                          seenHashes,
                                          seenObjects);

        if (result != nullptr) {
          TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, key);

          if (*str != 0) {
            // move the string pointer into the JSON object
            TRI_Insert4ArrayJson(TRI_UNKNOWN_MEM_ZONE, arrayJson, *str, str.length(), result, false);
            // this passes ownership for the utf8 string to the JSON object
            str.steal();
          }

          TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
        }
      }
    }

    seenObjects.pop_back();

    return arrayJson;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a json_t value
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ObjectToJson (v8::Isolate* isolate,
                              v8::Handle<v8::Value> const parameter) {
  set<int> seenHashes;
  vector<v8::Handle<v8::Object>> seenObjects;

  return ObjectToJson(isolate, parameter, seenHashes, seenObjects);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a string
////////////////////////////////////////////////////////////////////////////////

string TRI_ObjectToString (v8::Handle<v8::Value> const value) {
  TRI_Utf8ValueNFC utf8Value(TRI_UNKNOWN_MEM_ZONE, value);

  if (*utf8Value == 0) {
    return "";
  }
  else {
    return string(*utf8Value, utf8Value.length());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a character
////////////////////////////////////////////////////////////////////////////////

char TRI_ObjectToCharacter (v8::Handle<v8::Value> const value, bool& error) {
  error = false;

  if (! value->IsString() && ! value->IsStringObject()) {
    error = true;
    return '\0';
  }

  TRI_Utf8ValueNFC sep(TRI_UNKNOWN_MEM_ZONE, value->ToString());

  if (*sep == 0 || sep.length() != 1) {
    error = true;
    return '\0';
  }

  return (*sep)[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to an int64_t
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_ObjectToInt64 (v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return (int64_t) value->ToNumber()->Value();
  }

  if (value->IsNumberObject()) {
    v8::Handle<v8::NumberObject> no = v8::Handle<v8::NumberObject>::Cast(value);
    return (int64_t) no->NumberValue();
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a uint64_t
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_ObjectToUInt64 (v8::Handle<v8::Value> const value,
                             const bool allowStringConversion) {
  if (value->IsNumber()) {
    return (uint64_t) value->ToNumber()->Value();
  }

  if (value->IsNumberObject()) {
    v8::Handle<v8::NumberObject> no = v8::Handle<v8::NumberObject>::Cast(value);
    return (uint64_t) no->NumberValue();
  }

  if (allowStringConversion && value->IsString()) {
    v8::String::Utf8Value str(value);
    return StringUtils::uint64(*str, str.length());
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a double
////////////////////////////////////////////////////////////////////////////////

double TRI_ObjectToDouble (v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return value->ToNumber()->Value();
  }

  if (value->IsNumberObject()) {
    v8::Handle<v8::NumberObject> no = v8::Handle<v8::NumberObject>::Cast(value);
    return no->NumberValue();
  }

  return 0.0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a double with error handling
////////////////////////////////////////////////////////////////////////////////

double TRI_ObjectToDouble (v8::Handle<v8::Value> const value, bool& error) {
  error = false;

  if (value->IsNumber()) {
    return value->ToNumber()->Value();
  }

  if (value->IsNumberObject()) {
    v8::Handle<v8::NumberObject> no = v8::Handle<v8::NumberObject>::Cast(value);
    return no->NumberValue();
  }

  error = true;

  return 0.0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a boolean
////////////////////////////////////////////////////////////////////////////////

bool TRI_ObjectToBoolean (v8::Handle<v8::Value> const value) {
  if (value->IsBoolean()) {
    return value->ToBoolean()->Value();
  }
  else if (value->IsBooleanObject()) {
    v8::Handle<v8::BooleanObject> bo = v8::Handle<v8::BooleanObject>::Cast(value);
    return bo->BooleanValue();
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the V8 conversion module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Conversions (v8::Handle<v8::Context> context) {
  // nothing special to do here
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
