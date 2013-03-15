////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
///
/// @file
/// Declaration for shaped json objects.
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "shaped-json.h"

#include "BasicsC/associative.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "BasicsC/vector.h"
#include "ShapedJson/json-shaper.h"

// #define DEBUG_JSON_SHAPER 1

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static bool FillShapeValueJson (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json);
static TRI_json_t* JsonShapeData (TRI_shaper_t* shaper, TRI_shape_t const* shape, char const* data, uint64_t size);
static bool StringifyJsonShapeData (TRI_shaper_t* shaper, TRI_string_buffer_t* buffer, TRI_shape_t const* shape, char const* data, uint64_t size);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

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

  if (shape == NULL) {
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

static void PrintShapeValues (TRI_shape_value_t* values, size_t n) {
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

  assert(false);
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

  left = (TRI_shape_value_t const*) l;
  right = (TRI_shape_value_t const*) r;

  if (left->_fixedSized != right->_fixedSized) {
    return (left->_fixedSized ? 0 : 1) - (right->_fixedSized ? 0 : 1);
  }

  wl = WeightShapeType(left->_type);
  wr = WeightShapeType(right->_type);

  if (wl != wr) {
    return wl - wr;
  }

  return (int)(left->_aid - right->_aid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a null into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNull (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
  dst->_type = TRI_SHAPE_NULL;
  dst->_sid = shaper->_sidNull;
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
  dst->_sid = shaper->_sidBoolean;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_boolean_t);
  // no need to prefill dst->_value with 0, as it is overwritten directly afterwards
  dst->_value = (char*)(ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, false));

  if (dst->_value == NULL) {
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
  dst->_sid = shaper->_sidNumber;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_number_t);
  dst->_value = (char*)(ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

  if (dst->_value == NULL) {
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
    dst->_sid = shaper->_sidShortString;
    dst->_fixedSized = true;
    dst->_size = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
    dst->_value = (ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == NULL) {
      return false;
    }

    * ((TRI_shape_length_short_string_t*) ptr) = json->_value._string.length;

    memcpy(ptr + sizeof(TRI_shape_length_short_string_t),
           json->_value._string.data,
           json->_value._string.length);
  }
  else {
    dst->_type = TRI_SHAPE_LONG_STRING;
    dst->_sid = shaper->_sidLongString;
    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_long_string_t) + json->_value._string.length;
    dst->_value = (ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == NULL) {
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

static bool FillShapeValueList (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
  size_t i;
  size_t n;
  uint64_t total;

  TRI_shape_value_t* values;
  TRI_shape_value_t* p;
  TRI_shape_value_t* e;

  bool hs;
  bool hl;

  TRI_shape_sid_t s;
  TRI_shape_sid_t l;

  TRI_shape_sid_t* sids;
  TRI_shape_size_t* offsets;
  TRI_shape_size_t offset;

  TRI_shape_t const* found;

  char* ptr;

  // sanity checks
  assert(json->_type == TRI_JSON_LIST);

  // check for special case "empty list"
  n = json->_value._objects._length;

  if (n == 0) {
    dst->_type = TRI_SHAPE_LIST;
    dst->_sid = shaper->_sidList;

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t);
    dst->_value = (ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == NULL) {
      return false;
    }

    * (TRI_shape_length_list_t*) ptr = 0;

    return true;
  }

  // convert into TRI_shape_value_t array
  p = (values = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shape_value_t) * n, true));

  if (p == NULL) {
    return false;
  }

  total = 0;
  e = values + n;

  for (i = 0;  i < n;  ++i, ++p) {
    TRI_json_t* el = TRI_AtVector(&json->_value._objects, i);
    bool ok = FillShapeValueJson(shaper, p, el);

    if (! ok) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
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
    TRI_homogeneous_sized_list_shape_t* shape;

    shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_homogeneous_sized_list_shape_t), true);

    if (shape == NULL) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
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

    found = shaper->findShape(shaper, &shape->base);

    if (found == NULL) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      return false;
    }

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t) + total;
    dst->_value = (ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == NULL) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      return false;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = n;
    ptr += sizeof(TRI_shape_length_list_t);

    for (p = values;  p < e;  ++p) {
      memcpy(ptr, p->_value, (size_t) p->_size);
      ptr += p->_size;
    }
  }

  // homogeneous
  else if (hs) {
    TRI_homogeneous_list_shape_t* shape;

    shape = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_homogeneous_list_shape_t), true);

    if (shape == NULL) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
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

    found = shaper->findShape(shaper, &shape->base);

    if (found == NULL) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      return false;
    }

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    offset = sizeof(TRI_shape_length_list_t) + (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == NULL) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      TRI_Free(shaper->_memoryZone, shape);

      return false;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = n;
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
    dst->_sid = shaper->_sidList;

    offset =
      sizeof(TRI_shape_length_list_t)
      + n * sizeof(TRI_shape_sid_t)
      + (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

    if (dst->_value == NULL) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
          TRI_Free(shaper->_memoryZone, p->_value);
        }
      }

      TRI_Free(shaper->_memoryZone, values);
      return false;
    }

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = n;
    ptr += sizeof(TRI_shape_length_list_t);

    sids = (TRI_shape_sid_t*) ptr;
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
    if (p->_value != NULL) {
      TRI_Free(shaper->_memoryZone, p->_value);
    }
  }

  TRI_Free(shaper->_memoryZone, values);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json array into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueArray (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
  size_t n;
  size_t i;
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
  assert(json->_type == TRI_JSON_ARRAY);
  assert(json->_value._objects._length % 2 == 0);

  // number of attributes
  n = json->_value._objects._length / 2;

  // convert into TRI_shape_value_t array
  p = (values = TRI_Allocate(shaper->_memoryZone, n * sizeof(TRI_shape_value_t), true));

  if (p == NULL) {
    return false;
  }

  total = 0;
  f = 0;
  v = 0;

  for (i = 0;  i < n;  ++i, ++p) {
    TRI_json_t* key;
    TRI_json_t* val;
    bool ok;

    key = TRI_AtVector(&json->_value._objects, 2 * i);
    val = TRI_AtVector(&json->_value._objects, 2 * i + 1);

    // first find an identifier for the name
    p->_aid = shaper->findAttributeName(shaper, key->_value._string.data);

    // convert value
    if (p->_aid == 0) {
      ok = false;
    }
    else {
      ok = FillShapeValueJson(shaper, p, val);
    }

    if (! ok) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != NULL) {
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

  // now sort the shape entries
  TRI_SortShapeValues(values, n);

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

  a = (TRI_array_shape_t*) (ptr = TRI_Allocate(shaper->_memoryZone, i, true));

  if (ptr == NULL) {
    e = values + n;

    for (p = values;  p < e;  ++p) {
      if (p->_value != NULL) {
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
  dst->_value = (ptr = TRI_Allocate(shaper->_memoryZone, dst->_size, true));

  if (ptr == NULL) {
    e = values + n;

    for (p = values;  p < e;  ++p) {
      if (p->_value != NULL) {
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
    if (p->_value != NULL) {
      TRI_Free(shaper->_memoryZone, p->_value);
    }
  }

  TRI_Free(shaper->_memoryZone, values);

  // lookup this shape
  found = shaper->findShape(shaper, &a->base);

  if (found == NULL) {
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

static bool FillShapeValueJson (TRI_shaper_t* shaper, TRI_shape_value_t* dst, TRI_json_t const* json) {
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
      return FillShapeValueString(shaper, dst, json);

    case TRI_JSON_ARRAY:
      return FillShapeValueArray(shaper, dst, json);

    case TRI_JSON_LIST:
      return FillShapeValueList(shaper, dst, json);
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

  v = * (TRI_shape_number_t const*) data;

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

  array = TRI_CreateArrayJson(shaper->_memoryZone);

  if (array == NULL) {
    return NULL;
  }

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

  for (i = 0;  i < f;  ++i, ++sids, ++aids, ++offsetsF) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_aid_t aid = *aids;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    char const* name;
    TRI_json_t* element;

    offset = *offsetsF;
    subshape = shaper->lookupShapeId(shaper, sid);
    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == NULL) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == NULL) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsetsF[1] - offset);

    if (element == NULL) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_Insert2ArrayJson(shaper->_memoryZone, array, name, element);
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
    subshape = shaper->lookupShapeId(shaper, sid);
    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == NULL) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == NULL) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsetsV[1] - offset);

    if (element == NULL) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_Insert2ArrayJson(shaper->_memoryZone, array, name, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  if (shaper->_memoryZone->_failed) {
    return NULL;
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
  TRI_json_t* list;

  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsets;

  TRI_shape_length_list_t l;
  TRI_shape_length_list_t i;

  list = TRI_CreateListJson(shaper->_memoryZone);

  if (list == NULL) {
    return NULL;
  }

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  sids = (TRI_shape_sid_t const*) ptr;

  ptr += l * sizeof(TRI_shape_sid_t);
  offsets = (TRI_shape_size_t const*) ptr;

  for (i = 0;  i < l;  ++i, ++sids, ++offsets) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    TRI_json_t* element;

    offset = *offsets;
    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == NULL) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsets[1] - offset);

    if (element == NULL) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_PushBack2ListJson(list, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  if (shaper->_memoryZone->_failed) {
    return NULL;
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
  TRI_json_t* list;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t const* offsets;
  char const* ptr;

  s = (TRI_homogeneous_list_shape_t const*) shape;
  sid = s->_sidEntry;

  ptr = data;
  list = TRI_CreateListJson(shaper->_memoryZone);

  if (list == NULL) {
    return NULL;
  }

  l = * (TRI_shape_length_list_t const*) ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  offsets = (TRI_shape_size_t const*) ptr;

  for (i = 0;  i < l;  ++i, ++offsets) {
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    TRI_json_t* element;

    offset = *offsets;
    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == NULL) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsets[1] - offset);

    if (element == NULL) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_PushBack2ListJson(list, element);
    TRI_Free(shaper->_memoryZone, element);
  }

  if (shaper->_memoryZone->_failed) {
    return NULL;
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
  TRI_json_t* list;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t length;
  TRI_shape_size_t offset;
  char const* ptr;

  s = (TRI_homogeneous_sized_list_shape_t const*) shape;
  sid = s->_sidEntry;

  ptr = data;
  list = TRI_CreateListJson(shaper->_memoryZone);

  length = s->_sizeEntry;
  offset = sizeof(TRI_shape_length_list_t);

  l = * (TRI_shape_length_list_t const*) ptr;

  for (i = 0;  i < l;  ++i, offset += length) {
    TRI_shape_t const* subshape;
    TRI_json_t* element;

    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == NULL) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, length);

    if (element == NULL) {
      LOG_WARNING("cannot decode element for shape #%u", (unsigned int) sid);
      continue;
    }

    TRI_PushBack2ListJson(list, element);
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
  if (shape == NULL) {
    return NULL;
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

  return NULL;
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

  v = * (TRI_shape_number_t const*) data;
  // check for special values

  if (v != v) {
    // NaN
    res = TRI_AppendStringStringBuffer(buffer, "null");
  }
  else if (v == HUGE_VAL) {
    // +inf
    res = TRI_AppendStringStringBuffer(buffer, "null");
  }
  else if (v == -HUGE_VAL) {
    // -inf
    res = TRI_AppendStringStringBuffer(buffer, "null");
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
  TRI_shape_length_short_string_t l;
  char* unicoded;
  size_t out;
  int res;

  l = * (TRI_shape_length_short_string_t const*) data;
  data += sizeof(TRI_shape_length_short_string_t);

  res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  if (l > 1) {
    unicoded = TRI_EscapeUtf8StringZ(shaper->_memoryZone, data, (size_t) (l - 1), true, &out);

    if (unicoded == NULL) {
      return false;
    }

    res = TRI_AppendString2StringBuffer(buffer, unicoded, out);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    TRI_FreeString(shaper->_memoryZone, unicoded);
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
  TRI_shape_length_long_string_t l;
  char* unicoded;
  size_t out;
  int res;

  l = * (TRI_shape_length_long_string_t const*) data;
  data += sizeof(TRI_shape_length_long_string_t);

  res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  unicoded = TRI_EscapeUtf8StringZ(buffer->_memoryZone, data, l - 1, true, &out);

  if (unicoded == NULL) {
    return false;
  }

  res = TRI_AppendString2StringBuffer(buffer, unicoded, out);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeString(buffer->_memoryZone, unicoded);
    return false;
  }

  TRI_FreeString(buffer->_memoryZone, unicoded);

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
  bool first;
  char const* qtr;
  char* unicoded;
  size_t out;
  int res;

  s = (TRI_array_shape_t const*) shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;
  first = true;

  if (num != NULL) {
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
    subshape = shaper->lookupShapeId(shaper, sid);
    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == NULL) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == NULL) {
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

    unicoded = TRI_EscapeUtf8StringZ(shaper->_memoryZone, name, strlen(name), true, &out);

    if (unicoded == NULL) {
      return false;
    }

    res = TRI_AppendString2StringBuffer(buffer, unicoded, out);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    TRI_FreeString(shaper->_memoryZone, unicoded);

    res = TRI_AppendCharStringBuffer(buffer, '"');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    res = TRI_AppendCharStringBuffer(buffer, ':');

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
    subshape = shaper->lookupShapeId(shaper, sid);
    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == NULL) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == NULL) {
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

    unicoded = TRI_EscapeUtf8StringZ(shaper->_memoryZone, name, strlen(name), true, &out);

    if (unicoded == NULL) {
      return false;
    }

    res = TRI_AppendString2StringBuffer(buffer, unicoded, out);

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    TRI_FreeString(shaper->_memoryZone, unicoded);

    res = TRI_AppendCharStringBuffer(buffer, '"');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }

    res = TRI_AppendCharStringBuffer(buffer, ':');

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

  for (i = 0;  i < l;  ++i, ++sids, ++offsets) {
    TRI_shape_sid_t sid;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    bool ok;

    sid = *sids;
    offset = *offsets;
    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == NULL) {
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
  bool first;
  char const* ptr;
  int res;

  s = (TRI_homogeneous_list_shape_t const*) shape;
  sid = s->_sidEntry;

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
    TRI_shape_t const* subshape;
    bool ok;

    offset = *offsets;
    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == NULL) {
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
  bool first;
  char const* ptr;
  int res;

  s = (TRI_homogeneous_sized_list_shape_t const*) shape;
  sid = s->_sidEntry;

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
    TRI_shape_t const* subshape = shaper->lookupShapeId(shaper, sid);
    bool ok;

    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == NULL) {
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
/// @brief stringifies a data blob into a json object
////////////////////////////////////////////////////////////////////////////////

static bool StringifyJsonShapeData (TRI_shaper_t* shaper,
                                    TRI_string_buffer_t* buffer,
                                    TRI_shape_t const* shape,
                                    char const* data,
                                    uint64_t size) {
  if (shape == NULL) {
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
      return StringifyJsonShapeDataArray(shaper, buffer, shape, data, size, true, NULL);

    case TRI_SHAPE_LIST:
      return StringifyJsonShapeDataList(shaper, buffer, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_LIST:
      return StringifyJsonShapeDataHomogeneousList(shaper, buffer, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      return StringifyJsonShapeDataHomogeneousSizedList(shaper, buffer, shape, data, size);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a deep copy of a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_CopyShapedJson (TRI_shaper_t* shaper, TRI_shaped_json_t* oldShapedJson) {
  TRI_shaped_json_t* newShapedJson;
  int res;

  if (oldShapedJson == NULL) {
    return NULL;
  }

  newShapedJson = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shaped_json_t), true);

  if (newShapedJson == NULL) {
    return NULL;
  }

  newShapedJson->_sid  = oldShapedJson->_sid;
  res = TRI_CopyToBlob(shaper->_memoryZone, &(newShapedJson->_data), &(oldShapedJson->_data));

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(shaper->_memoryZone, newShapedJson);
    return NULL;
  }

  return newShapedJson;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShapedJson (TRI_shaper_t* shaper, TRI_shaped_json_t* shaped) {
  TRI_DestroyBlob(shaper->_memoryZone, &shaped->_data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapedJson (TRI_shaper_t* shaper, TRI_shaped_json_t* shaped) {
  TRI_DestroyShapedJson(shaper, shaped);
  TRI_Free(shaper->_memoryZone, shaped);
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
/// @brief sorts a list of TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

void TRI_SortShapeValues (TRI_shape_value_t* values, size_t n) {
  qsort(values, n, sizeof(TRI_shape_value_t), SortShapeValuesFunc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonJson (TRI_shaper_t* shaper, TRI_json_t const* json) {
  TRI_shaped_json_t* shaped;
  TRI_shape_value_t dst;
  bool ok;

  dst._value = 0;
  ok = FillShapeValueJson(shaper, &dst, json);

  if (! ok) {
    return NULL;
  }

#ifdef DEBUG_JSON_SHAPER
  printf("shape\n-----\n");
  TRI_PrintShape(shaper, shaper->lookupShapeId(shaper, dst._sid), 0);
  printf("\n");
#endif

  // no need to prefill shaped with 0's as all attributes are set directly afterwards
  shaped = TRI_Allocate(shaper->_memoryZone, sizeof(TRI_shaped_json_t), false);

  if (shaped == NULL) {
    TRI_Free(shaper->_memoryZone, dst._value);
    return NULL;
  }

  shaped->_sid = dst._sid;
  shaped->_data.length = (uint32_t) dst._size;
  shaped->_data.data = dst._value;

  return shaped;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a shaped json objet into a json object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonShapedJson (TRI_shaper_t* shaper, TRI_shaped_json_t const* shaped) {
  TRI_shape_t const* shape;

  shape = shaper->lookupShapeId(shaper, shaped->_sid);

  if (shape == NULL) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) shaped->_sid);
    return NULL;
  }

  return JsonShapeData(shaper, shape, shaped->_data.data, shaped->_data.length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyShapedJson (TRI_shaper_t* shaper,
                              TRI_string_buffer_t* buffer,
                              TRI_shaped_json_t const* shaped) {
  TRI_shape_t const* shape;

  shape = shaper->lookupShapeId(shaper, shaped->_sid);

  if (shape == NULL) {
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

  if (shape == NULL) {
    return false;
  }

  if (augment == NULL || augment->_type != TRI_JSON_ARRAY || shape->_type != TRI_SHAPE_ARRAY) {
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
                                const TRI_shaped_json_t* const shapedJson,
                                char** value,
                                size_t* length) {
  if (shape->_type == TRI_SHAPE_SHORT_STRING) {
    TRI_shape_length_short_string_t l;
    char* data;

    data = shapedJson->_data.data;
    l = * (TRI_shape_length_short_string_t const*) data;
    data += sizeof(TRI_shape_length_short_string_t);
    *length = l - 1;
    *value = data;

    return true;
  }
  else if (shape->_type == TRI_SHAPE_LONG_STRING) {
    TRI_shape_length_long_string_t l;
    char* data;

    data = shapedJson->_data.data;
    l = * (TRI_shape_length_long_string_t const*) data;
    data += sizeof(TRI_shape_length_long_string_t);
    *length = l - 1;
    *value = data;

    return true;
  }

  // no string type
  *value = NULL;
  *length = 0;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
