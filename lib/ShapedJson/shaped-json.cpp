////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
///
/// @file
/// Declaration for shaped json objects.
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

#include "shaped-json.h"

#include "Basics/associative.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Basics/string-buffer.h"
#include "Basics/tri-strings.h"
#include "Basics/vector.h"
#include "ShapedJson/json-shaper.h"

// #define DEBUG_JSON_SHAPER 1

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static bool FillShapeValueJson (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json, size_t, bool);
static TRI_json_t* JsonShapeData (TRI_shaper_t* shaper, TRI_shape_t const* shape, char const* data, uint64_t size);
static bool StringifyJsonShapeData (TRI_shaper_t* shaper, TRI_string_buffer_t* buffer, TRI_shape_t const* shape, char const* data, uint64_t size);

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a TRI_shape_t for debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_PrintShape (TRI_shaper_t* shaper, TRI_shape_t const* shape, int indent) {
  TRI_array_shape_t const* array;
  TRI_homogeneous_list_shape_t const* homList;
  TRI_homogeneous_sized_list_shape_t const* homSizedList;
  TRI_shape_aid_t const* aids;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsets;
  char const* ptr;
  size_t i;
  uint64_t n;

  if (shape == nullptr) {
    printf("%*sUNKNOWN\n", indent, "");
    return;
  }

  switch (shape->_type) {
    case TRI_SHAPE_NULL:
      printf("%*sNULL sid: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) shape->_dataSize);
      break;

    case TRI_SHAPE_BOOLEAN:
      printf("%*sBOOLEAN sid: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) shape->_dataSize);
      break;

    case TRI_SHAPE_NUMBER:
      printf("%*sNUMBER sid: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) shape->_dataSize);
      break;

    case TRI_SHAPE_SHORT_STRING:
      printf("%*sSHORT STRING sid: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) shape->_dataSize);
      break;

    case TRI_SHAPE_LONG_STRING:
      printf("%*sLONG STRING sid: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) shape->_dataSize);
      break;

    case TRI_SHAPE_ARRAY:
      array = (TRI_array_shape_t const*) shape;
      n = array->_fixedEntries + array->_variableEntries;

      printf("%*sARRAY sid: %u, fixed: %u, variable: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) array->_fixedEntries,
             (unsigned int) array->_variableEntries,
             (unsigned int) shape->_dataSize);

      ptr = (char const*) shape;
      ptr += sizeof(TRI_array_shape_t);

      sids = (TRI_shape_sid_t const*) ptr;
      ptr += n * sizeof(TRI_shape_sid_t);

      aids = (TRI_shape_aid_t const*) ptr;
      ptr += n * sizeof(TRI_shape_aid_t);

      offsets = (TRI_shape_size_t const*) ptr;

      for (i = 0;  i < array->_fixedEntries;  ++i, ++sids, ++aids, ++offsets) {
        char const* m = shaper->lookupAttributeId(shaper, *aids);

        if (n == 0) {
          m = "[NULL]";
        }

        printf("%*sENTRY FIX #%u aid: %u (%s), sid: %u, offset: %u - %u\n", indent + 2, "",
               (unsigned int) i,
               (unsigned int) *aids,
               m,
               (unsigned int) *sids,
               (unsigned int) offsets[0],
               (unsigned int) offsets[1]);

        TRI_PrintShape(shaper, shaper->lookupShapeId(shaper, *sids), indent + 4);
      }

      for (i = 0;  i < array->_variableEntries;  ++i, ++sids, ++aids) {
        char const* m = shaper->lookupAttributeId(shaper, *aids);

        if (n == 0) {
          m = "[NULL]";
        }

        printf("%*sENTRY VAR #%u aid: %u (%s), sid: %u\n", indent + 2, "",
               (unsigned int) i,
               (unsigned int) *aids,
               m,
               (unsigned int) *sids);

        TRI_PrintShape(shaper, shaper->lookupShapeId(shaper, *sids), indent + 4);
      }

      break;

    case TRI_SHAPE_LIST:
      printf("%*sLIST sid: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) shape->_dataSize);
      break;

    case TRI_SHAPE_HOMOGENEOUS_LIST:
      homList = (TRI_homogeneous_list_shape_t const*) shape;

      printf("%*sHOMOGENEOUS LIST sid: %u, entry sid: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) homList->_sidEntry,
             (unsigned int) shape->_dataSize);
      break;

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      homSizedList = (TRI_homogeneous_sized_list_shape_t const*) shape;

      printf("%*sHOMOGENEOUS SIZED LIST sid: %u, entry sid: %u, entry size: %u, data size: %u\n", indent, "",
             (unsigned int) shape->_sid,
             (unsigned int) homSizedList->_sidEntry,
             (unsigned int) homSizedList->_sizeEntry,
             (unsigned int) shape->_dataSize);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a list of TRI_shape_value_t for debugging
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_JSON_SHAPER

static void PrintShapeValues (TRI_shape_value_t* values,
                              size_t n) {
  TRI_shape_value_t* p;
  TRI_shape_value_t* e;

  p = values;
  e = values + n;

  for (;  p < e;  ++p) {
    switch (p->_type) {
      case TRI_SHAPE_NULL:
        printf("NULL aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size);
        break;

      case TRI_SHAPE_BOOLEAN:
        printf("BOOLEAN aid: %u, sid: %u, fixed: %s, size: %u, value: %s",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size,
               *((TRI_shape_boolean_t const*) p->_value) ? "true" : "false");
        break;

      case TRI_SHAPE_NUMBER:
        printf("NUMBER aid: %u, sid: %u, fixed: %s, size: %u, value: %f",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size,
               *((TRI_shape_number_t const*) p->_value));
        break;

      case TRI_SHAPE_SHORT_STRING:
        printf("SHORT STRING aid: %u, sid: %u, fixed: %s, size: %u, value: %s",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size,
               p->_value + sizeof(TRI_shape_length_short_string_t));
        break;

      case TRI_SHAPE_LONG_STRING:
        printf("LONG STRING aid: %u, sid: %u, fixed: %s, size: %u, value: %s",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size,
               p->_value + sizeof(TRI_shape_length_long_string_t));
        break;

      case TRI_SHAPE_ARRAY:
        printf("ARRAY aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size);
        break;

      case TRI_SHAPE_LIST:
        printf("LIST aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size);
        break;

      case TRI_SHAPE_HOMOGENEOUS_LIST:
        printf("HOMOGENEOUS LIST aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size);
        break;

      case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        printf("HOMOGENEOUS SIZED LIST aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int) p->_aid,
               (unsigned int) p->_sid,
               p->_fixedSized ? "yes" : "no",
               (unsigned int) p->_size);
        break;

      default:
        printf("unknown");
        break;
    }

    printf("\n");
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief weight function for TRI_shape_type_t
////////////////////////////////////////////////////////////////////////////////

static int WeightShapeType (TRI_shape_type_t type) {
  switch (type) {

    case TRI_SHAPE_NULL:                   return 100;
    case TRI_SHAPE_BOOLEAN:                return 200;
    case TRI_SHAPE_NUMBER:                 return 300;
    case TRI_SHAPE_SHORT_STRING:           return 400;
    case TRI_SHAPE_LONG_STRING:            return 500;
    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: return 600;
    case TRI_SHAPE_ARRAY:                  return 700;
    case TRI_SHAPE_LIST:                   return 800;
    case TRI_SHAPE_HOMOGENEOUS_LIST:       return 900;
  }

  LOG_ERROR("invalid shape type: %d\n", (int) type);

  TRI_ASSERT(false);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort function for TRI_shape_value_t
///
/// Note that the fixed sized entries *must* come first.
////////////////////////////////////////////////////////////////////////////////

static int SortShapeValuesFunc (void const* l, void const* r) {
  TRI_shape_value_t const* left;
  TRI_shape_value_t const* right;
  int wl;
  int wr;

  left  = (TRI_shape_value_t const*) l;
  right = (TRI_shape_value_t const*) r;

  if (left->_fixedSized != right->_fixedSized) {
    return (left->_fixedSized ? 0 : 1) - (right->_fixedSized ? 0 : 1);
  }

  wl = WeightShapeType(left->_type);
  wr = WeightShapeType(right->_type);

  if (wl != wr) {
    return wl - wr;
  }

  return (int) (left->_aid - right->_aid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a null into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNull (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
  dst->_type = TRI_SHAPE_NULL;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_NULL);
  dst->_fixedSized = true;
  dst->_size = 0;
  dst->_value = 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a boolean into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueBoolean (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
  TRI_shape_boolean_t* ptr;

  dst->_type = TRI_SHAPE_BOOLEAN;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_BOOLEAN);
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_boolean_t);
  // no need to prefill dst->_value with 0, as it is overwritten directly afterwards
  dst->_value = (char*) (ptr = static_cast<TRI_shape_boolean_t*>(TRI_Allocate(shaper->_memoryZone, dst->_size, false)));

  if (dst->_value == nullptr) {
    return false;
  }

  *ptr = json->_value._boolean ? 1 : 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a number into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNumber (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
  TRI_shape_number_t* ptr;

  dst->_type = TRI_SHAPE_NUMBER;
  dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER);
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_number_t);
  dst->_value = (char*) (ptr = static_cast<TRI_shape_number_t*>(TRI_Allocate(shaper->_memoryZone, dst->_size, false)));

  if (dst->_value == nullptr) {
    return false;
  }

  *ptr = json->_value._number;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a string into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueString (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
  char* ptr;

  if (json->_value._string.length <= TRI_SHAPE_SHORT_STRING_CUT) { // includes '\0'
    dst->_type = TRI_SHAPE_SHORT_STRING;
    dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_SHORT_STRING);
    dst->_fixedSized = true;
    dst->_size = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
    // must fill with 0's because the string might be shorter, and we might use the
    // full length for memcmp comparisons!!
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, true)));

    if (dst->_value == nullptr) {
      return false;
    }

    * ((TRI_shape_length_short_string_t*) ptr) = json->_value._string.length;

    memcpy(ptr + sizeof(TRI_shape_length_short_string_t),
           json->_value._string.data,
           json->_value._string.length);
  }
  else {
    dst->_type = TRI_SHAPE_LONG_STRING;
    dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_LONG_STRING);
    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_long_string_t) + json->_value._string.length;
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, false)));

    if (dst->_value == nullptr) {
      return false;
    }

    * ((TRI_shape_length_long_string_t*) ptr) = json->_value._string.length;

    memcpy(ptr + sizeof(TRI_shape_length_long_string_t),
           json->_value._string.data,
           json->_value._string.length);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json list into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueList (TRI_shaper_t* shaper,
                                TRI_shape_value_t* dst,
                                TRI_json_t const* json,
                                size_t level,
                                bool create) {
  size_t i, n;
  uint64_t total;

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

  // sanity checks
  TRI_ASSERT(json->_type == TRI_JSON_ARRAY);

  // check for special case "empty list"
  n = json->_value._objects._length;

  if (n == 0) {
    dst->_type = TRI_SHAPE_LIST;
    dst->_sid = TRI_LookupBasicSidShaper(TRI_SHAPE_LIST);

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t);
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, false)));

    if (dst->_value == nullptr) {
      return false;
    }

    * (TRI_shape_length_list_t*) ptr = 0;

    return true;
  }

  // convert into TRI_shape_value_t array
  p = (values = static_cast<TRI_shape_value_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shape_value_t) * n, true)));

  if (p == nullptr) {
    return false;
  }

  total = 0;
  e = values + n;

  for (i = 0;  i < n;  ++i, ++p) {
    TRI_json_t const* el = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
    bool ok = FillShapeValueJson(shaper, p, el, level + 1, create);

    if (! ok) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);

      return false;
    }

    total += p->_size;
  }

  // check if this list is homogeneous
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
    TRI_homogeneous_sized_list_shape_t* shape = static_cast<TRI_homogeneous_sized_list_shape_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_homogeneous_sized_list_shape_t), true));

    if (shape == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);

      return false;
    }

    shape->base._size = sizeof(TRI_homogeneous_sized_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_SIZED_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;
    shape->_sizeEntry = l;

    // note: if 'found' is not a nullptr, the shaper will have freed variable 'shape'!
    found = shaper->findShape(shaper, &shape->base, create);

    if (found == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      return false;
    }

    TRI_ASSERT(found != nullptr);

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t) + total;
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, true)));

    if (dst->_value == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);

      return false;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = (uint32_t) n;
    ptr += sizeof(TRI_shape_length_list_t);

    for (p = values;  p < e;  ++p) {
      memcpy(ptr, p->_value, (size_t) p->_size);
      ptr += p->_size;
    }
  }

  // homogeneous
  else if (hs) {
    TRI_homogeneous_list_shape_t* shape = static_cast<TRI_homogeneous_list_shape_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_homogeneous_list_shape_t), true));

    if (shape == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);

      return false;
    }

    shape->base._size = sizeof(TRI_homogeneous_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;

    // note: if 'found' is not a nullptr, the shaper will have freed variable 'shape'!
    found = shaper->findShape(shaper, &shape->base, create);

    if (found == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      return false;
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

      return false;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = (uint32_t) n;
    ptr += sizeof(TRI_shape_length_list_t);

    offsets = (TRI_shape_size_t*) ptr;
    ptr += (n + 1) * sizeof(TRI_shape_size_t);

    for (p = values;  p < e;  ++p) {
      *offsets++ = offset;
      offset += p->_size;

      memcpy(ptr, p->_value, (size_t) p->_size);
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
    dst->_value = (ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, dst->_size, true)));

    if (dst->_value == nullptr) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      return false;
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

      memcpy(ptr, p->_value, (size_t) p->_size);
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
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json array into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueArray (TRI_shaper_t* shaper,
                                 TRI_shape_value_t* dst,
                                 TRI_json_t const* json,
                                 size_t level,
                                 bool create) {
  size_t i, n;
  uint64_t total;

  size_t f;
  size_t v;

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

  // sanity checks
  TRI_ASSERT(json->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(json->_value._objects._length % 2 == 0);

  // number of attributes
  n = json->_value._objects._length / 2;

  // convert into TRI_shape_value_t array
  p = (values = static_cast<TRI_shape_value_t*>(TRI_Allocate(shaper->_memoryZone, n * sizeof(TRI_shape_value_t), true)));

  if (p == nullptr) {
    return false;
  }

  total = 0;
  f = 0;
  v = 0;

  for (i = 0;  i < n;  ++i, ++p) {
    TRI_json_t const* key = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, 2 * i));
    TRI_ASSERT(key != nullptr);
    TRI_ASSERT(key->_type == TRI_JSON_STRING);

    char const* k = key->_value._string.data;

    if (k == nullptr ||
        key->_value._string.length == 1) {
      // empty attribute name
      p--;
      continue;
    }

    if (*k == '_' && level == 0) {
      // on top level, strip reserved attributes before shaping
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

    // first find an identifier for the name
    p->_aid = shaper->findOrCreateAttributeByName(shaper, k);

    // convert value
    bool ok;
    if (p->_aid == 0) {
      ok = false;
    }
    else {
      TRI_json_t const* val = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, 2 * i + 1));
      TRI_ASSERT(val != nullptr);

      ok = FillShapeValueJson(shaper, p, val, level + 1, create);
    }

    if (! ok) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != nullptr) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      return false;
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

  // add variable offset table size
  total += (v + 1) * sizeof(TRI_shape_size_t);

  // now adjust n because we might have excluded empty attributes
  n = f + v;

  // now sort the shape entries
  if (n > 1) {
    TRI_SortShapeValues(values, n);
  }

#ifdef DEBUG_JSON_SHAPER
  printf("shape values\n------------\ntotal: %u, fixed: %u, variable: %u\n",
         (unsigned int) n,
         (unsigned int) f,
         (unsigned int) v);
  PrintShapeValues(values, n);
  printf("\n");
#endif

  // generate shape structure
  i =
    sizeof(TRI_array_shape_t)
    + n * sizeof(TRI_shape_sid_t)
    + n * sizeof(TRI_shape_aid_t)
    + (f + 1) * sizeof(TRI_shape_size_t);

  a = reinterpret_cast<TRI_array_shape_t*>(ptr = static_cast<char*>(TRI_Allocate(shaper->_memoryZone, i, true)));

  if (ptr == nullptr) {
    e = values + n;

    for (p = values;  p < e;  ++p) {
      if (p->_value != nullptr) {
        TRI_Free(shaper->_memoryZone, p->_value);
      }
    }

    TRI_Free(shaper->_memoryZone, values);

    return false;
  }

  a->base._type = TRI_SHAPE_ARRAY;
  a->base._size = i;
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

    return false;
  }

  // array of offsets for variable part (within the value)
  offsetsV = (TRI_shape_size_t*) ptr;
  ptr += (v + 1) * sizeof(TRI_shape_size_t);

  // and fill in attributes
  e = values + n;

  for (p = values;  p < e;  ++p) {
    *aids++ = p->_aid;
    *sids++ = p->_sid;

    memcpy(ptr, p->_value, (size_t) p->_size);
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
    TRI_Free(shaper->_memoryZone, dst->_value);
    TRI_Free(shaper->_memoryZone, a);
    return false;
  }

  // and finally add the sid
  dst->_sid = found->_sid;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueJson (TRI_shaper_t* shaper,
                                TRI_shape_value_t* dst,
                                TRI_json_t const* json,
                                size_t level,
                                bool create) {
  switch (json->_type) {
    case TRI_JSON_UNUSED:
      return false;

    case TRI_JSON_NULL:
      return FillShapeValueNull(shaper, dst, json);

    case TRI_JSON_BOOLEAN:
      return FillShapeValueBoolean(shaper, dst, json);

    case TRI_JSON_NUMBER:
      return FillShapeValueNumber(shaper, dst, json);

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE:
      return FillShapeValueString(shaper, dst, json);

    case TRI_JSON_OBJECT:
      return FillShapeValueArray(shaper, dst, json, level, create);

    case TRI_JSON_ARRAY:
      return FillShapeValueList(shaper, dst, json, level, create);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data null blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataNull (TRI_shaper_t* shaper,
                                      TRI_shape_t const* shape,
                                      char const* data,
                                      uint64_t size) {
  return TRI_CreateNullJson(shaper->_memoryZone);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data boolean blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataBoolean (TRI_shaper_t* shaper,
                                         TRI_shape_t const* shape,
                                         char const* data,
                                         uint64_t size) {
  bool v;

  v = (* (TRI_shape_boolean_t const*) data) != 0;

  return TRI_CreateBooleanJson(shaper->_memoryZone, v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data number blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataNumber (TRI_shaper_t* shaper,
                                        TRI_shape_t const* shape,
                                        char const* data,
                                        uint64_t size) {
  TRI_shape_number_t v;

  v = * (TRI_shape_number_t const*) (void const*) data;

  return TRI_CreateNumberJson(shaper->_memoryZone, v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data short string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataShortString (TRI_shaper_t* shaper,
                                             TRI_shape_t const* shape,
                                             char const* data,
                                             uint64_t size) {
  TRI_shape_length_short_string_t l;

  l = * (TRI_shape_length_short_string_t const*) data;
  data += sizeof(TRI_shape_length_short_string_t);

  return TRI_CreateString2CopyJson(shaper->_memoryZone, data, (size_t) (l - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data long string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataLongString (TRI_shaper_t* shaper,
                                            TRI_shape_t const* shape,
                                            char const* data,
                                            uint64_t size) {
  TRI_shape_length_long_string_t l;

  l = * (TRI_shape_length_long_string_t const*) data;
  data += sizeof(TRI_shape_length_long_string_t);

  return TRI_CreateString2CopyJson(shaper->_memoryZone, data, l - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data array blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataArray (TRI_shaper_t* shaper,
                                       TRI_shape_t const* shape,
                                       char const* data,
                                       uint64_t size) {
  TRI_array_shape_t const* s;
  TRI_shape_size_t f;
  TRI_shape_size_t v;
  TRI_shape_size_t n;
  TRI_shape_size_t i;

  char const* qtr;
  TRI_json_t* array;

  TRI_shape_sid_t const* sids;
  TRI_shape_aid_t const* aids;
  TRI_shape_size_t const* offsetsF;
  TRI_shape_size_t const* offsetsV;

  shape_cache_t shapeCache;

  s = (TRI_array_shape_t const*) shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;

  // create an array with the appropriate size
  array = TRI_CreateObject2Json(shaper->_memoryZone, (size_t) n);

  if (array == nullptr) {
    return nullptr;
  }

  qtr = (char const*) shape;
  qtr += sizeof(TRI_array_shape_t);

  sids = (TRI_shape_sid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_sid_t);

  aids = (TRI_shape_aid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_aid_t);

  offsetsF = (TRI_shape_size_t const*) qtr;

  shapeCache._sid = 0;
  shapeCache._shape = nullptr;

  for (i = 0;  i < f;  ++i, ++sids, ++aids, ++offsetsF) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_aid_t aid = *aids;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    char const* name;
    TRI_json_t* element;

    offset = *offsetsF;

    // use last sid if in cache
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

    name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsetsF[1] - offset);

    if (element == nullptr) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_Insert2ObjectJson(shaper->_memoryZone, array, name, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  offsetsV = (TRI_shape_size_t const*) data;

  for (i = 0;  i < v;  ++i, ++sids, ++aids, ++offsetsV) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_aid_t aid = *aids;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    char const* name;
    TRI_json_t* element;

    offset = *offsetsV;

    // use last sid if in cache
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

    name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsetsV[1] - offset);

    if (element == nullptr) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_Insert2ObjectJson(shaper->_memoryZone, array, name, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  if (shaper->_memoryZone->_failed) {
    TRI_FreeJson(shaper->_memoryZone, array);

    return nullptr;
  }

  return array;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataList (TRI_shaper_t* shaper,
                                      TRI_shape_t const* shape,
                                      char const* data,
                                      uint64_t size) {
  char const* ptr;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsets;
  TRI_shape_length_list_t l;
  TRI_shape_length_list_t i;
  shape_cache_t shapeCache;

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  // create a list with the appropriate size
  TRI_json_t* list = TRI_CreateArray2Json(shaper->_memoryZone, (size_t) l);

  if (list == nullptr) {
    return nullptr;
  }

  ptr += sizeof(TRI_shape_length_list_t);
  sids = (TRI_shape_sid_t const*) ptr;

  ptr += l * sizeof(TRI_shape_sid_t);
  offsets = (TRI_shape_size_t const*) ptr;

  shapeCache._sid = 0;
  shapeCache._shape = nullptr;

  for (i = 0;  i < l;  ++i, ++sids, ++offsets) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    TRI_json_t* element;

    offset = *offsets;

    // use last sid if in cache
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

    element = JsonShapeData(shaper, subshape, data + offset, offsets[1] - offset);

    if (element == nullptr) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_PushBack2ArrayJson(list, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  if (shaper->_memoryZone->_failed) {
    TRI_FreeJson(shaper->_memoryZone, list);

    return nullptr;
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataHomogeneousList (TRI_shaper_t* shaper,
                                                 TRI_shape_t const* shape,
                                                 char const* data,
                                                 uint64_t size) {
  TRI_homogeneous_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t const* offsets;
  char const* ptr;

  s = (TRI_homogeneous_list_shape_t const*) shape;
  sid = s->_sidEntry;

  TRI_shape_t const* subshape = shaper->lookupShapeId(shaper, sid);

  if (subshape == nullptr) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) sid);

    return nullptr;
  }

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  // create a list with the appropriate size
  TRI_json_t* list = TRI_CreateArray2Json(shaper->_memoryZone, (size_t) l);

  if (list == nullptr) {
    return nullptr;
  }

  ptr += sizeof(TRI_shape_length_list_t);
  offsets = (TRI_shape_size_t const*) ptr;

  for (i = 0;  i < l;  ++i, ++offsets) {
    TRI_shape_size_t offset;
    TRI_json_t* element;

    offset = *offsets;

    element = JsonShapeData(shaper, subshape, data + offset, offsets[1] - offset);

    if (element == nullptr) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_PushBack2ArrayJson(list, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  if (shaper->_memoryZone->_failed) {
    TRI_FreeJson(shaper->_memoryZone, list);

    return nullptr;
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous sized list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeDataHomogeneousSizedList (TRI_shaper_t* shaper,
                                                      TRI_shape_t const* shape,
                                                      char const* data,
                                                      uint64_t size) {
  TRI_homogeneous_sized_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t length;
  TRI_shape_size_t offset;
  char const* ptr;

  s = (TRI_homogeneous_sized_list_shape_t const*) shape;
  sid = s->_sidEntry;

  TRI_shape_t const* subshape = shaper->lookupShapeId(shaper, sid);

  if (subshape == nullptr) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) sid);

    return nullptr;
  }

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  // create a list with the appropriate size
  TRI_json_t* list = TRI_CreateArray2Json(shaper->_memoryZone, (size_t) l);

  if (list == nullptr) {
    return nullptr;
  }

  length = s->_sizeEntry;
  offset = sizeof(TRI_shape_length_list_t);

  for (i = 0;  i < l;  ++i, offset += length) {
    TRI_json_t* element;

    element = JsonShapeData(shaper, subshape, data + offset, length);

    if (element == nullptr) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_PushBack2ArrayJson(list, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data blob into a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonShapeData (TRI_shaper_t* shaper,
                                  TRI_shape_t const* shape,
                                  char const* data,
                                  uint64_t size) {
  if (shape == nullptr) {
    return nullptr;
  }

  switch (shape->_type) {
    case TRI_SHAPE_NULL:
      return JsonShapeDataNull(shaper, shape, data, size);

    case TRI_SHAPE_BOOLEAN:
      return JsonShapeDataBoolean(shaper, shape, data, size);

    case TRI_SHAPE_NUMBER:
      return JsonShapeDataNumber(shaper, shape, data, size);

    case TRI_SHAPE_SHORT_STRING:
      return JsonShapeDataShortString(shaper, shape, data, size);

    case TRI_SHAPE_LONG_STRING:
      return JsonShapeDataLongString(shaper, shape, data, size);

    case TRI_SHAPE_ARRAY:
      return JsonShapeDataArray(shaper, shape, data, size);

    case TRI_SHAPE_LIST:
      return JsonShapeDataList(shaper, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_LIST:
      return JsonShapeDataHomogeneousList(shaper, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      return JsonShapeDataHomogeneousSizedList(shaper, shape, data, size);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data null blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataNull (TRI_shaper_t* shaper,
                                        TRI_string_buffer_t* buffer,
                                        TRI_shape_t const* shape,
                                        char const* data,
                                        uint64_t size) {
  int res;

  res = TRI_AppendString2StringBuffer(buffer, "null", 4);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data boolean blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataBoolean (TRI_shaper_t* shaper,
                                           TRI_string_buffer_t* buffer,
                                           TRI_shape_t const* shape,
                                           char const* data,
                                           uint64_t size) {
  bool v;
  int res;

  v = (* (TRI_shape_boolean_t const*) data) != 0;

  if (v) {
    res = TRI_AppendString2StringBuffer(buffer, "true", 4);
  }
  else {
    res = TRI_AppendString2StringBuffer(buffer, "false", 5);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data number blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataNumber (TRI_shaper_t* shaper,
                                          TRI_string_buffer_t* buffer,
                                          TRI_shape_t const* shape,
                                          char const* data,
                                          uint64_t size) {
  TRI_shape_number_t v;
  int res;

  v = * (TRI_shape_number_t const*) (void const*) data;
  // check for special values

  // yes, this is intentional
  if (v != v) {
    // NaN
    res = TRI_AppendString2StringBuffer(buffer, "null", 4);
  }
  else if (v == HUGE_VAL) {
    // +inf
    res = TRI_AppendString2StringBuffer(buffer, "null", 4);
  }
  else if (v == -HUGE_VAL) {
    // -inf
    res = TRI_AppendString2StringBuffer(buffer, "null", 4);
  }
  else {
    res = TRI_AppendDoubleStringBuffer(buffer, v);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data short string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataShortString (TRI_shaper_t* shaper,
                                               TRI_string_buffer_t* buffer,
                                               TRI_shape_t const* shape,
                                               char const* data,
                                               uint64_t size) {
  int res;

  data += sizeof(TRI_shape_length_short_string_t);

  res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  res = TRI_AppendJsonEncodedStringStringBuffer(buffer, data, true);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data long string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataLongString (TRI_shaper_t* shaper,
                                              TRI_string_buffer_t* buffer,
                                              TRI_shape_t const* shape,
                                              char const* data,
                                              uint64_t size) {
  int res;

  data += sizeof(TRI_shape_length_long_string_t);

  res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  res = TRI_AppendJsonEncodedStringStringBuffer(buffer, data, true);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data array blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataArray (TRI_shaper_t* shaper,
                                         TRI_string_buffer_t* buffer,
                                         TRI_shape_t const* shape,
                                         char const* data,
                                         uint64_t size,
                                         bool braces,
                                         uint64_t* num) {
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
  bool first;
  char const* qtr;
  int res;

  s = (TRI_array_shape_t const*) shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;
  first = true;

  if (num != nullptr) {
    *num = n;
  }

  qtr = (char const*) shape;

  if (braces) {
    res = TRI_AppendCharStringBuffer(buffer, '{');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }
  }

  qtr += sizeof(TRI_array_shape_t);

  sids = (TRI_shape_sid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_sid_t);

  aids = (TRI_shape_aid_t const*) qtr;
  qtr += n * sizeof(TRI_shape_aid_t);

  offsetsF = (TRI_shape_size_t const*) qtr;

  shapeCache._sid   = 0;
  shapeCache._shape = nullptr;

  for (i = 0;  i < f;  ++i, ++sids, ++aids, ++offsetsF) {
    TRI_shape_aid_t aid;
    TRI_shape_sid_t sid;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    bool ok;
    char const* name;

    sid = *sids;
    aid = *aids;
    offset = *offsetsF;

    // use last sid if in cache
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

    name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    if (first) {
      first = false;
    }
    else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    res = TRI_AppendCharStringBuffer(buffer, '"');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    res = TRI_AppendJsonEncodedStringStringBuffer(buffer, name, true);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    res = TRI_AppendString2StringBuffer(buffer, "\":", 2);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    ok = StringifyJsonShapeData(shaper, buffer, subshape, data + offset, offsetsF[1] - offset);

    if (! ok) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }
  }

  offsetsV = (TRI_shape_size_t const*) data;

  for (i = 0;  i < v;  ++i, ++sids, ++aids, ++offsetsV) {
    TRI_shape_sid_t sid;
    TRI_shape_aid_t aid;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    char const* name;
    bool ok;

    sid = *sids;
    aid = *aids;
    offset = *offsetsV;

    // use last sid if in cache
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

    name = shaper->lookupAttributeId(shaper, aid);

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    if (first) {
      first = false;
    }
    else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    res = TRI_AppendCharStringBuffer(buffer, '"');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    res = TRI_AppendJsonEncodedStringStringBuffer(buffer, name, true);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    res = TRI_AppendString2StringBuffer(buffer, "\":", 2);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    ok = StringifyJsonShapeData(shaper, buffer, subshape, data + offset, offsetsV[1] - offset);

    if (! ok) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }
  }

  if (braces) {
    res = TRI_AppendCharStringBuffer(buffer, '}');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataList (TRI_shaper_t* shaper,
                                        TRI_string_buffer_t* buffer,
                                        TRI_shape_t const* shape,
                                        char const* data,
                                        uint64_t size) {
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsets;
  shape_cache_t shapeCache;
  bool first;
  char const* ptr;
  int res;

  ptr = data;
  first = true;
  l = * (TRI_shape_length_list_t const*) ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  sids = (TRI_shape_sid_t const*) ptr;

  ptr += l * sizeof(TRI_shape_sid_t);
  offsets = (TRI_shape_size_t const*) ptr;

  res = TRI_AppendCharStringBuffer(buffer, '[');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  shapeCache._sid   = 0;
  shapeCache._shape = nullptr;

  for (i = 0;  i < l;  ++i, ++sids, ++offsets) {
    TRI_shape_sid_t sid;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    bool ok;

    sid = *sids;
    offset = *offsets;

    // use last sid if in cache
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

    if (first) {
      first = false;
    }
    else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    ok = StringifyJsonShapeData(shaper, buffer, subshape, data + offset, offsets[1] - offset);

    if (! ok) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }
  }

  res = TRI_AppendCharStringBuffer(buffer, ']');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data homogeneous list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataHomogeneousList (TRI_shaper_t* shaper,
                                                   TRI_string_buffer_t* buffer,
                                                   TRI_shape_t const* shape,
                                                   char const* data,
                                                   uint64_t size) {
  TRI_homogeneous_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t const* offsets;
  TRI_shape_t const* subshape;
  bool first;
  char const* ptr;
  int res;

  s = (TRI_homogeneous_list_shape_t const*) shape;
  sid = s->_sidEntry;

  subshape = shaper->lookupShapeId(shaper, sid);

  if (subshape == nullptr) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) sid);

    return false;
  }

  ptr = data;
  first = true;

  l = * (TRI_shape_length_list_t const*) ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  offsets = (TRI_shape_size_t const*) ptr;

  res = TRI_AppendCharStringBuffer(buffer, '[');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  for (i = 0;  i < l;  ++i, ++offsets) {
    TRI_shape_size_t offset;
    bool ok;

    offset = *offsets;

    if (first) {
      first = false;
    }
    else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    ok = StringifyJsonShapeData(shaper, buffer, subshape, data + offset, offsets[1] - offset);

    if (! ok) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }
  }

  res = TRI_AppendCharStringBuffer(buffer, ']');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data homogeneous sized list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeDataHomogeneousSizedList (TRI_shaper_t* shaper,
                                                        TRI_string_buffer_t* buffer,
                                                        TRI_shape_t const* shape,
                                                        char const* data,
                                                        uint64_t size) {
  TRI_homogeneous_sized_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t length;
  TRI_shape_size_t offset;
  TRI_shape_t const* subshape;
  bool first;
  char const* ptr;
  int res;

  s = (TRI_homogeneous_sized_list_shape_t const*) shape;
  sid = s->_sidEntry;
  subshape = shaper->lookupShapeId(shaper, sid);

  if (subshape == nullptr) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) sid);

    return false;
  }

  ptr = data;
  first = true;

  length = s->_sizeEntry;
  offset = sizeof(TRI_shape_length_list_t);

  l = * (TRI_shape_length_list_t const*) ptr;

  res = TRI_AppendCharStringBuffer(buffer, '[');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  for (i = 0;  i < l;  ++i, offset += length) {
    bool ok;

    if (first) {
      first = false;
    }
    else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    ok = StringifyJsonShapeData(shaper, buffer, subshape, data + offset, length);

    if (! ok) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }
  }

  res = TRI_AppendCharStringBuffer(buffer, ']');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data blob into a string buffer
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeData (TRI_shaper_t* shaper,
                                    TRI_string_buffer_t* buffer,
                                    TRI_shape_t const* shape,
                                    char const* data,
                                    uint64_t size) {
  if (shape == nullptr) {
    return false;
  }

  switch (shape->_type) {
    case TRI_SHAPE_NULL:
      return StringifyJsonShapeDataNull(shaper, buffer, shape, data, size);

    case TRI_SHAPE_BOOLEAN:
      return StringifyJsonShapeDataBoolean(shaper, buffer, shape, data, size);

    case TRI_SHAPE_NUMBER:
      return StringifyJsonShapeDataNumber(shaper, buffer, shape, data, size);

    case TRI_SHAPE_SHORT_STRING:
      return StringifyJsonShapeDataShortString(shaper, buffer, shape, data, size);

    case TRI_SHAPE_LONG_STRING:
      return StringifyJsonShapeDataLongString(shaper, buffer, shape, data, size);

    case TRI_SHAPE_ARRAY:
      return StringifyJsonShapeDataArray(shaper, buffer, shape, data, size, true, nullptr);

    case TRI_SHAPE_LIST:
      return StringifyJsonShapeDataList(shaper, buffer, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_LIST:
      return StringifyJsonShapeDataHomogeneousList(shaper, buffer, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      return StringifyJsonShapeDataHomogeneousSizedList(shaper, buffer, shape, data, size);
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a deep copy of a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_CopyShapedJson (TRI_shaper_t* shaper, TRI_shaped_json_t* oldShapedJson) {
  if (oldShapedJson == nullptr) {
    return nullptr;
  }

  TRI_shaped_json_t* newShapedJson = static_cast<TRI_shaped_json_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shaped_json_t), false));

  if (newShapedJson == nullptr) {
    return nullptr;;
  }

  newShapedJson->_sid = oldShapedJson->_sid;
  int res = TRI_CopyToBlob(shaper->_memoryZone, &newShapedJson->_data, &oldShapedJson->_data);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(shaper->_memoryZone, newShapedJson);
    return nullptr;
  }

  return newShapedJson;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShapedJson (TRI_memory_zone_t* zone,
                            TRI_shaped_json_t* shaped) {
  TRI_DestroyBlob(zone, &shaped->_data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapedJson (TRI_memory_zone_t* zone,
                         TRI_shaped_json_t* shaped) {
  TRI_DestroyShapedJson(zone, shaped);
  TRI_Free(zone, shaped);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a list of TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

void TRI_SortShapeValues (TRI_shape_value_t* values,
                          size_t n) {
  qsort(values, n, sizeof(TRI_shape_value_t), SortShapeValuesFunc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonJson (TRI_shaper_t* shaper,
                                       TRI_json_t const* json,
                                       bool create) {
  TRI_shape_value_t dst;

  dst._value = 0;
  bool ok = FillShapeValueJson(shaper, &dst, json, 0, create);

  if (! ok) {
    return nullptr;
  }

#ifdef DEBUG_JSON_SHAPER
  printf("shape\n-----\n");
  TRI_PrintShape(shaper, shaper->lookupShapeId(shaper, dst._sid), 0);
  printf("\n");
#endif

  // no need to prefill shaped with 0's as all attributes are set directly afterwards
  TRI_shaped_json_t* shaped = static_cast<TRI_shaped_json_t*>(TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shaped_json_t), false));

  if (shaped == nullptr) {
    TRI_Free(shaper->_memoryZone, dst._value);
    return nullptr;
  }

  shaped->_sid = dst._sid;
  shaped->_data.length = (uint32_t) dst._size;
  shaped->_data.data = dst._value;

  return shaped;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a shaped json object into a json object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonShapedJson (TRI_shaper_t* shaper,
                                TRI_shaped_json_t const* shaped) {
  TRI_shape_t const* shape;

  shape = shaper->lookupShapeId(shaper, shaped->_sid);

  if (shape == nullptr) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) shaped->_sid);
    return nullptr;
  }

  return JsonShapeData(shaper, shape, shaped->_data.data, shaped->_data.length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer, without the outer braces
/// this can only be used to stringify shapes of type array
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyArrayShapedJson (TRI_shaper_t* shaper,
                                   TRI_string_buffer_t* buffer,
                                   TRI_shaped_json_t const* shaped,
                                   bool prepend) {
  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, shaped->_sid);

  if (shape == nullptr || shape->_type != TRI_SHAPE_ARRAY) {
    return false;
  }

  if (prepend) {
    TRI_array_shape_t const* s;

    s = (TRI_array_shape_t const*) shape;
    if (s->_fixedEntries + s->_variableEntries > 0) {
      TRI_AppendCharStringBuffer(buffer, ',');
    }
  }

  uint64_t num;
  bool ok = StringifyJsonShapeDataArray(shaper, buffer, shape, shaped->_data.data, shaped->_data.length, false, &num);
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyShapedJson (TRI_shaper_t* shaper,
                              TRI_string_buffer_t* buffer,
                              TRI_shaped_json_t const* shaped) {
  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, shaped->_sid);

  if (shape == nullptr) {
    return false;
  }

  return StringifyJsonShapeData(shaper, buffer, shape, shaped->_data.data, shaped->_data.length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyAugmentedShapedJson (TRI_shaper_t* shaper,
                                       struct TRI_string_buffer_s* buffer,
                                       TRI_shaped_json_t const* shaped,
                                       TRI_json_t const* augment) {
  TRI_shape_t const* shape;
  bool ok;
  uint64_t num;
  int res;

  shape = shaper->lookupShapeId(shaper, shaped->_sid);

  if (shape == nullptr) {
    return false;
  }

  if (augment == nullptr || augment->_type != TRI_JSON_OBJECT || shape->_type != TRI_SHAPE_ARRAY) {
    return StringifyJsonShapeData(shaper, buffer, shape, shaped->_data.data, shaped->_data.length);
  }

  res = TRI_AppendCharStringBuffer(buffer, '{');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  ok = StringifyJsonShapeDataArray(shaper, buffer, shape, shaped->_data.data, shaped->_data.length, false, &num);

  if (0 < num) {
    res = TRI_AppendCharStringBuffer(buffer, ',');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }
  }

  if (! ok) {
    return false;
  }

  res = TRI_Stringify2Json(buffer, augment);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  res = TRI_AppendCharStringBuffer(buffer, '}');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthListShapedJson (TRI_list_shape_t const* shape,
                                 TRI_shaped_json_t const* json) {
  return * (TRI_shape_length_list_t*) json->_data.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtListShapedJson (TRI_list_shape_t const* shape,
                           TRI_shaped_json_t const* json,
                           size_t position,
                           TRI_shaped_json_t* result) {
  TRI_shape_length_list_t n;
  TRI_shape_sid_t* sids;
  TRI_shape_size_t* offsets;
  char const* ptr;

  ptr = json->_data.data;
  n = * (TRI_shape_length_list_t*) ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  sids = (TRI_shape_sid_t*) ptr;

  ptr += n * sizeof(TRI_shape_sid_t);
  offsets = (TRI_shape_size_t*) ptr;

  result->_sid = sids[position];
  result->_data.data = ((char*) json->_data.data) + offsets[position];
  result->_data.length = (uint32_t)(offsets[position + 1] - offsets[position]);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a homogeneous list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthHomogeneousListShapedJson (TRI_homogeneous_list_shape_t const* shape,
                                            TRI_shaped_json_t const* json) {
  return * (TRI_shape_length_list_t*) json->_data.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a homogeneous list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtHomogeneousListShapedJson (TRI_homogeneous_list_shape_t const* shape,
                                      TRI_shaped_json_t const* json,
                                      size_t position,
                                      TRI_shaped_json_t* result) {
  TRI_shape_length_list_t n;
  TRI_shape_size_t* offsets;
  char const* ptr;

  ptr = json->_data.data;
  n = * (TRI_shape_length_list_t*) ptr;

  if (n <= position) {
    return false;
  }

  ptr += sizeof(TRI_shape_length_list_t);
  offsets = (TRI_shape_size_t*) ptr;

  result->_sid = shape->_sidEntry;
  result->_data.data = ((char*) json->_data.data) + offsets[position];
  result->_data.length = (uint32_t)(offsets[position + 1] - offsets[position]);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a homogeneous sized list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthHomogeneousSizedListShapedJson (TRI_homogeneous_sized_list_shape_t const* shape,
                                                 TRI_shaped_json_t const* json) {
  return * (TRI_shape_length_list_t*) json->_data.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a homogeneous sized list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtHomogeneousSizedListShapedJson (TRI_homogeneous_sized_list_shape_t const* shape,
                                           TRI_shaped_json_t const* json,
                                           size_t position,
                                           TRI_shaped_json_t* result) {
  TRI_shape_length_list_t n;
  char* ptr;

  ptr = json->_data.data;
  n = * (TRI_shape_length_list_t*) ptr;

  if (n <= position) {
    return false;
  }

  ptr += sizeof(TRI_shape_length_list_t);

  result->_sid = shape->_sidEntry;
  result->_data.data = ptr + (shape->_sizeEntry * position);
  result->_data.length = (uint32_t) shape->_sizeEntry;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string value encoded in a shaped json
/// this will return the pointer to the string and the string length in the
/// variables passed by reference
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringValueShapedJson (const TRI_shape_t* const shape,
                                const char* data,
                                char** value,
                                size_t* length) {
  if (shape->_type == TRI_SHAPE_SHORT_STRING) {
    TRI_shape_length_short_string_t l;

    l = * (TRI_shape_length_short_string_t const*) data;
    data += sizeof(TRI_shape_length_short_string_t);
    *length = l - 1;
    *value = (char*) data;

    return true;
  }
  else if (shape->_type == TRI_SHAPE_LONG_STRING) {
    TRI_shape_length_long_string_t l;

    l = * (TRI_shape_length_long_string_t const*) data;
    data += sizeof(TRI_shape_length_long_string_t);
    *length = l - 1;
    *value = (char*) data;

    return true;
  }

  // no string type
  *value = nullptr;
  *length = 0;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data blob into a string buffer
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyJsonShapeData (TRI_shaper_t* shaper,
                                 TRI_string_buffer_t* buffer,
                                 TRI_shape_t const* shape,
                                 char const* data,
                                 uint64_t size) {
  return StringifyJsonShapeData(shaper, buffer, shape, data, size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over a shaped json array, using a callback function
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateShapeDataArray (TRI_shaper_t* shaper,
                                TRI_shape_t const* shape,
                                char const* data,
                                bool (*filter)(TRI_shaper_t*, TRI_shape_t const*, char const*, char const*, uint64_t, void*),
                                void* ptr) {
  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsetsF;
  TRI_shape_size_t const* offsetsV;
  TRI_shape_size_t f;
  TRI_shape_size_t i;
  TRI_shape_size_t n;
  TRI_shape_size_t v;
  TRI_shape_sid_t cachedSid;
  TRI_shape_t const* cachedShape;
  char const* qtr;

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

  cachedSid = 0;
  cachedShape = nullptr;

  for (i = 0;  i < f;  ++i, ++sids, ++aids, ++offsetsF) {
    TRI_shape_aid_t aid;
    TRI_shape_sid_t sid;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    char const* name;

    sid = *sids;
    aid = *aids;
    offset = *offsetsF;

    // use last sid if in cache
    if (sid == cachedSid && cachedSid > 0) {
      subshape = cachedShape;
    }
    else {
      cachedShape = subshape = shaper->lookupShapeId(shaper, sid);
      cachedSid = sid;
    }

    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == nullptr) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    if (! filter(shaper, subshape, name, data + offset, offsetsF[1] - offset, ptr)) {
      return;
    }
  }

  offsetsV = (TRI_shape_size_t const*) data;

  for (i = 0;  i < v;  ++i, ++sids, ++aids, ++offsetsV) {
    TRI_shape_sid_t sid;
    TRI_shape_aid_t aid;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    char const* name;

    sid = *sids;
    aid = *aids;
    offset = *offsetsV;

    // use last sid if in cache
    if (sid == cachedSid && cachedSid > 0) {
      subshape = cachedShape;
    }
    else {
      cachedShape = subshape = shaper->lookupShapeId(shaper, sid);
      cachedSid = sid;
    }

    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == nullptr) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == nullptr) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    if (! filter(shaper, subshape, name, data + offset, offsetsV[1] - offset, ptr)) {
      return;
    }
  }

  return;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a list of TRI_shape_value_t for debugging
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_JSON_SHAPER

void TRI_PrintShapeValues (TRI_shape_value_t* values,
                           size_t n) {
  PrintShapeValues(values, n);
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
