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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "shaped-json.h"

#include "Basics/hashes.h"
#include "Basics/Logger.h"
#include "Basics/StringBuffer.h"
#include "Basics/tri-strings.h"
#include "Basics/vector.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/Legends.h"
#include "VocBase/VocShaper.h"

// #define DEBUG_JSON_SHAPER 1

static bool FillShapeValueJson(VocShaper*, TRI_shape_value_t*,
                               TRI_json_t const*, size_t, bool);

static int JsonShapeData(VocShaper*, TRI_shape_t const*, TRI_json_t*,
                         char const*, uint64_t);

template <typename T>
static bool StringifyJsonShapeData(T*, TRI_string_buffer_t*, TRI_shape_t const*,
                                   char const*, uint64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief shape cache (caches pointer to last shape)
////////////////////////////////////////////////////////////////////////////////

typedef struct shape_cache_s {
  TRI_shape_sid_t _sid;
  TRI_shape_t const* _shape;
} shape_cache_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a TRI_shape_t for debugging
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_JSON_SHAPER

void TRI_PrintShape(VocShaper* shaper, TRI_shape_t const* shape, int indent) {
  if (shape == nullptr) {
    printf("%*sUNKNOWN\n", indent, "");
    return;
  }

  switch (shape->_type) {
    case TRI_SHAPE_NULL:
      printf("%*sNULL sid: %u, data size: %u\n", indent, "",
             (unsigned int)shape->_sid, (unsigned int)shape->_dataSize);
      break;

    case TRI_SHAPE_BOOLEAN:
      printf("%*sBOOLEAN sid: %u, data size: %u\n", indent, "",
             (unsigned int)shape->_sid, (unsigned int)shape->_dataSize);
      break;

    case TRI_SHAPE_NUMBER:
      printf("%*sNUMBER sid: %u, data size: %u\n", indent, "",
             (unsigned int)shape->_sid, (unsigned int)shape->_dataSize);
      break;

    case TRI_SHAPE_SHORT_STRING:
      printf("%*sSHORT STRING sid: %u, data size: %u\n", indent, "",
             (unsigned int)shape->_sid, (unsigned int)shape->_dataSize);
      break;

    case TRI_SHAPE_LONG_STRING:
      printf("%*sLONG STRING sid: %u, data size: %u\n", indent, "",
             (unsigned int)shape->_sid, (unsigned int)shape->_dataSize);
      break;

    case TRI_SHAPE_ARRAY: {
      TRI_array_shape_t const* array = (TRI_array_shape_t const*)shape;
      uint64_t n = array->_fixedEntries + array->_variableEntries;

      printf("%*sARRAY sid: %u, fixed: %u, variable: %u, data size: %u\n",
             indent, "", (unsigned int)shape->_sid,
             (unsigned int)array->_fixedEntries,
             (unsigned int)array->_variableEntries,
             (unsigned int)shape->_dataSize);

      char const* ptr = (char const*)shape;
      ptr += sizeof(TRI_array_shape_t);

      TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*)ptr;
      ptr += n * sizeof(TRI_shape_sid_t);

      TRI_shape_aid_t const* aids = (TRI_shape_aid_t const*)ptr;
      ptr += n * sizeof(TRI_shape_aid_t);

      TRI_shape_size_t const* offsets = (TRI_shape_size_t const*)ptr;

      for (size_t i = 0; i < array->_fixedEntries;
           ++i, ++sids, ++aids, ++offsets) {
        char const* m = shaper->lookupAttributeId(*aids);

        if (n == 0) {
          m = "[NULL]";
        }

        printf("%*sENTRY FIX #%u aid: %u (%s), sid: %u, offset: %u - %u\n",
               indent + 2, "", (unsigned int)i, (unsigned int)*aids, m,
               (unsigned int)*sids, (unsigned int)offsets[0],
               (unsigned int)offsets[1]);

        TRI_PrintShape(shaper, shaper->lookupShapeId(*sids), indent + 4);
      }

      for (size_t i = 0; i < array->_variableEntries; ++i, ++sids, ++aids) {
        char const* m = shaper->lookupAttributeId(*aids);

        if (n == 0) {
          m = "[NULL]";
        }

        printf("%*sENTRY VAR #%u aid: %u (%s), sid: %u\n", indent + 2, "",
               (unsigned int)i, (unsigned int)*aids, m, (unsigned int)*sids);

        TRI_PrintShape(shaper, shaper->lookupShapeId(*sids), indent + 4);
      }
      break;
    }

    case TRI_SHAPE_LIST: {
      printf("%*sLIST sid: %u, data size: %u\n", indent, "",
             (unsigned int)shape->_sid, (unsigned int)shape->_dataSize);
      break;
    }

    case TRI_SHAPE_HOMOGENEOUS_LIST: {
      TRI_homogeneous_list_shape_t const* homList =
          (TRI_homogeneous_list_shape_t const*)shape;

      printf("%*sHOMOGENEOUS LIST sid: %u, entry sid: %u, data size: %u\n",
             indent, "", (unsigned int)shape->_sid,
             (unsigned int)homList->_sidEntry, (unsigned int)shape->_dataSize);
      break;
    }

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
      TRI_homogeneous_sized_list_shape_t const* homSizedList =
          (TRI_homogeneous_sized_list_shape_t const*)shape;

      printf(
          "%*sHOMOGENEOUS SIZED LIST sid: %u, entry sid: %u, entry size: %u, "
          "data size: %u\n",
          indent, "", (unsigned int)shape->_sid,
          (unsigned int)homSizedList->_sidEntry,
          (unsigned int)homSizedList->_sizeEntry,
          (unsigned int)shape->_dataSize);
      break;
    }
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a list of TRI_shape_value_t for debugging
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_JSON_SHAPER

static void PrintShapeValues(TRI_shape_value_t* values, size_t n) {
  TRI_shape_value_t* p = values;
  TRI_shape_value_t* e = values + n;

  for (; p < e; ++p) {
    switch (p->_type) {
      case TRI_SHAPE_NULL:
        printf("NULL aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size);
        break;

      case TRI_SHAPE_BOOLEAN:
        printf("BOOLEAN aid: %u, sid: %u, fixed: %s, size: %u, value: %s",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size,
               *((TRI_shape_boolean_t const*)p->_value) ? "true" : "false");
        break;

      case TRI_SHAPE_NUMBER:
        printf("NUMBER aid: %u, sid: %u, fixed: %s, size: %u, value: %f",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size,
               *((TRI_shape_number_t const*)p->_value));
        break;

      case TRI_SHAPE_SHORT_STRING:
        printf("SHORT STRING aid: %u, sid: %u, fixed: %s, size: %u, value: %s",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size,
               p->_value + sizeof(TRI_shape_length_short_string_t));
        break;

      case TRI_SHAPE_LONG_STRING:
        printf("LONG STRING aid: %u, sid: %u, fixed: %s, size: %u, value: %s",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size,
               p->_value + sizeof(TRI_shape_length_long_string_t));
        break;

      case TRI_SHAPE_ARRAY:
        printf("ARRAY aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size);
        break;

      case TRI_SHAPE_LIST:
        printf("LIST aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size);
        break;

      case TRI_SHAPE_HOMOGENEOUS_LIST:
        printf("HOMOGENEOUS LIST aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size);
        break;

      case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        printf("HOMOGENEOUS SIZED LIST aid: %u, sid: %u, fixed: %s, size: %u",
               (unsigned int)p->_aid, (unsigned int)p->_sid,
               p->_fixedSized ? "yes" : "no", (unsigned int)p->_size);
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

static int WeightShapeType(TRI_shape_type_t type) {
  switch (type) {
    case TRI_SHAPE_NULL:
      return 100;
    case TRI_SHAPE_BOOLEAN:
      return 200;
    case TRI_SHAPE_NUMBER:
      return 300;
    case TRI_SHAPE_SHORT_STRING:
      return 400;
    case TRI_SHAPE_LONG_STRING:
      return 500;
    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      return 600;
    case TRI_SHAPE_ARRAY:
      return 700;
    case TRI_SHAPE_LIST:
      return 800;
    case TRI_SHAPE_HOMOGENEOUS_LIST:
      return 900;
  }

  LOG(ERR) << "invalid shape type: " << type << "\n";

  TRI_ASSERT(false);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort function for TRI_shape_value_t
///
/// Note that the fixed sized entries *must* come first.
////////////////////////////////////////////////////////////////////////////////

static int SortShapeValuesFunc(void const* l, void const* r) {
  auto left = static_cast<TRI_shape_value_t const*>(l);
  auto right = static_cast<TRI_shape_value_t const*>(r);

  if (left->_fixedSized != right->_fixedSized) {
    return (left->_fixedSized ? 0 : 1) - (right->_fixedSized ? 0 : 1);
  }

  int wl = WeightShapeType(left->_type);
  int wr = WeightShapeType(right->_type);

  if (wl != wr) {
    return wl - wr;
  }

  return (int)(left->_aid - right->_aid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a null into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNull(VocShaper* shaper, TRI_shape_value_t* dst,
                               TRI_json_t const* json) {
  dst->_type = TRI_SHAPE_NULL;
  dst->_sid = BasicShapes::TRI_SHAPE_SID_NULL;
  dst->_fixedSized = true;
  dst->_size = 0;
  dst->_value = 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a boolean into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueBoolean(VocShaper* shaper, TRI_shape_value_t* dst,
                                  TRI_json_t const* json) {
  TRI_shape_boolean_t* ptr;

  dst->_type = TRI_SHAPE_BOOLEAN;
  dst->_sid = BasicShapes::TRI_SHAPE_SID_BOOLEAN;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_boolean_t);
  // no need to prefill dst->_value with 0, as it is overwritten directly
  // afterwards
  dst->_value = (char*)(ptr = static_cast<TRI_shape_boolean_t*>(TRI_Allocate(
                            shaper->memoryZone(), dst->_size, false)));

  if (dst->_value == nullptr) {
    return false;
  }

  *ptr = json->_value._boolean ? 1 : 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a number into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNumber(VocShaper* shaper, TRI_shape_value_t* dst,
                                 TRI_json_t const* json) {
  TRI_shape_number_t* ptr;

  dst->_type = TRI_SHAPE_NUMBER;
  dst->_sid = BasicShapes::TRI_SHAPE_SID_NUMBER;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_number_t);
  dst->_value = (char*)(ptr = static_cast<TRI_shape_number_t*>(TRI_Allocate(
                            shaper->memoryZone(), dst->_size, false)));

  if (dst->_value == nullptr) {
    return false;
  }

  *ptr = json->_value._number;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a string into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueString(VocShaper* shaper, TRI_shape_value_t* dst,
                                 TRI_json_t const* json) {
  char* ptr;

  if (json->_value._string.length <=
      TRI_SHAPE_SHORT_STRING_CUT) {  // includes '\0'
    dst->_type = TRI_SHAPE_SHORT_STRING;
    dst->_sid = BasicShapes::TRI_SHAPE_SID_SHORT_STRING;
    dst->_fixedSized = true;
    dst->_size =
        sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
    // must fill with 0's because the string might be shorter, and we might use
    // the
    // full length for memcmp comparisons!!
    dst->_value = (ptr = static_cast<char*>(
                       TRI_Allocate(shaper->memoryZone(), dst->_size, true)));

    if (dst->_value == nullptr) {
      return false;
    }

    *((TRI_shape_length_short_string_t*)ptr) = json->_value._string.length;

    memcpy(ptr + sizeof(TRI_shape_length_short_string_t),
           json->_value._string.data, json->_value._string.length);
  } else {
    dst->_type = TRI_SHAPE_LONG_STRING;
    dst->_sid = BasicShapes::TRI_SHAPE_SID_LONG_STRING;
    dst->_fixedSized = false;
    dst->_size =
        sizeof(TRI_shape_length_long_string_t) + json->_value._string.length;
    dst->_value = (ptr = static_cast<char*>(
                       TRI_Allocate(shaper->memoryZone(), dst->_size, false)));

    if (dst->_value == nullptr) {
      return false;
    }

    *((TRI_shape_length_long_string_t*)ptr) = json->_value._string.length;

    memcpy(ptr + sizeof(TRI_shape_length_long_string_t),
           json->_value._string.data, json->_value._string.length);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json list into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueList(VocShaper* shaper, TRI_shape_value_t* dst,
                               TRI_json_t const* json, size_t level,
                               bool create) {
  TRI_shape_sid_t s;
  TRI_shape_sid_t l;

  TRI_shape_size_t* offsets;
  TRI_shape_size_t offset;

  char* ptr;

  // sanity checks
  TRI_ASSERT(json->_type == TRI_JSON_ARRAY);

  // check for special case "empty list"
  size_t const n = TRI_LengthArrayJson(json);

  if (n == 0) {
    dst->_type = TRI_SHAPE_LIST;
    dst->_sid = BasicShapes::TRI_SHAPE_SID_LIST;
    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t);
    dst->_value = (ptr = static_cast<char*>(
                       TRI_Allocate(shaper->memoryZone(), dst->_size, false)));

    if (dst->_value == nullptr) {
      return false;
    }

    *(TRI_shape_length_list_t*)ptr = 0;

    return true;
  }

  // helper function to free shape values
  auto freeShapeValues = [](TRI_memory_zone_t* zone, TRI_shape_value_t* values,
                            TRI_shape_value_t* end) {
    TRI_shape_value_t* p = values;
    TRI_shape_value_t* e = end;

    for (; p < e; ++p) {
      if (p->_value != nullptr) {
        TRI_Free(zone, p->_value);
      }
    }

    TRI_Free(zone, values);
  };

  // convert into TRI_shape_value_t array
  TRI_shape_value_t* values = static_cast<TRI_shape_value_t*>(
      TRI_Allocate(shaper->memoryZone(), sizeof(TRI_shape_value_t) * n, true));

  if (values == nullptr) {
    return false;
  }

  uint64_t total = 0;

  TRI_shape_value_t* p = values;

  for (size_t i = 0; i < n; ++i, ++p) {
    TRI_json_t const* el =
        static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
    bool ok = FillShapeValueJson(shaper, p, el, level + 1, create);

    if (!ok) {
      freeShapeValues(shaper->memoryZone(), values, p);
      return false;
    }

    total += p->_size;
  }

  // check if this list is homogeneous
  bool hs = true;
  bool hl = true;

  s = values[0]._sid;
  l = values[0]._size;

  p = values;
  TRI_shape_value_t* const e = values + n;  // end does not change

  for (; p < e; ++p) {
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
    TRI_homogeneous_sized_list_shape_t* shape =
        static_cast<TRI_homogeneous_sized_list_shape_t*>(
            TRI_Allocate(shaper->memoryZone(),
                         sizeof(TRI_homogeneous_sized_list_shape_t), true));

    if (shape == nullptr) {
      freeShapeValues(shaper->memoryZone(), values, e);
      return false;
    }

    shape->base._size = sizeof(TRI_homogeneous_sized_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_SIZED_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;
    shape->_sizeEntry = l;

    // note: if 'found' is not a nullptr, the shaper will have freed variable
    // 'shape'!
    TRI_shape_t const* found = shaper->findShape(&shape->base, create);

    if (found == nullptr) {
      freeShapeValues(shaper->memoryZone(), values, e);
      TRI_Free(shaper->memoryZone(), shape);
      return false;
    }

    TRI_ASSERT(found != nullptr);

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t) + total;
    dst->_value = (ptr = static_cast<char*>(
                       TRI_Allocate(shaper->memoryZone(), dst->_size, true)));

    if (dst->_value == nullptr) {
      freeShapeValues(shaper->memoryZone(), values, e);
      return false;
    }

    // copy sub-objects into data space
    *(TRI_shape_length_list_t*)ptr = (uint32_t)n;
    ptr += sizeof(TRI_shape_length_list_t);

    for (p = values; p < e; ++p) {
      TRI_ASSERT(p->_value != nullptr || p->_size == 0);
      if (p->_value != nullptr) {
        memcpy(ptr, p->_value, (size_t)p->_size);
      }
      ptr += p->_size;
    }
  }

  // homogeneous
  else if (hs) {
    TRI_homogeneous_list_shape_t* shape =
        static_cast<TRI_homogeneous_list_shape_t*>(TRI_Allocate(
            shaper->memoryZone(), sizeof(TRI_homogeneous_list_shape_t), true));

    if (shape == nullptr) {
      freeShapeValues(shaper->memoryZone(), values, e);
      return false;
    }

    shape->base._size = sizeof(TRI_homogeneous_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;

    // note: if 'found' is not a nullptr, the shaper will have freed variable
    // 'shape'!
    TRI_shape_t const* found = shaper->findShape(&shape->base, create);

    if (found == nullptr) {
      freeShapeValues(shaper->memoryZone(), values, e);
      TRI_Free(shaper->memoryZone(), shape);
      return false;
    }

    TRI_ASSERT(found != nullptr);

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    offset =
        sizeof(TRI_shape_length_list_t) + (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = static_cast<char*>(
                       TRI_Allocate(shaper->memoryZone(), dst->_size, true)));

    if (dst->_value == nullptr) {
      freeShapeValues(shaper->memoryZone(), values, e);
      return false;
    }

    // copy sub-objects into data space
    *(TRI_shape_length_list_t*)ptr = (uint32_t)n;
    ptr += sizeof(TRI_shape_length_list_t);

    offsets = (TRI_shape_size_t*)ptr;
    ptr += (n + 1) * sizeof(TRI_shape_size_t);

    for (p = values; p < e; ++p) {
      *offsets++ = offset;
      offset += p->_size;

      TRI_ASSERT(p->_value != nullptr || p->_size == 0);
      if (p->_value != nullptr) {
        memcpy(ptr, p->_value, (size_t)p->_size);
      }
      ptr += p->_size;
    }

    *offsets = offset;
  }

  // in-homogeneous
  else {
    dst->_type = TRI_SHAPE_LIST;
    dst->_sid = BasicShapes::TRI_SHAPE_SID_LIST;

    offset = sizeof(TRI_shape_length_list_t) + n * sizeof(TRI_shape_sid_t) +
             (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = static_cast<char*>(
                       TRI_Allocate(shaper->memoryZone(), dst->_size, true)));

    if (dst->_value == nullptr) {
      freeShapeValues(shaper->memoryZone(), values, e);
      return false;
    }

    // copy sub-objects into data space
    *(TRI_shape_length_list_t*)ptr = (TRI_shape_length_list_t)n;
    ptr += sizeof(TRI_shape_length_list_t);

    TRI_shape_sid_t* sids = (TRI_shape_sid_t*)ptr;
    ptr += n * sizeof(TRI_shape_sid_t);

    offsets = (TRI_shape_size_t*)ptr;
    ptr += (n + 1) * sizeof(TRI_shape_size_t);

    for (p = values; p < e; ++p) {
      *sids++ = p->_sid;

      *offsets++ = offset;
      offset += p->_size;

      TRI_ASSERT(p->_value != nullptr || p->_size == 0);
      if (p->_value != nullptr) {
        memcpy(ptr, p->_value, (size_t)p->_size);
      }
      ptr += p->_size;
    }

    *offsets = offset;
  }

  // free TRI_shape_value_t array
  freeShapeValues(shaper->memoryZone(), values, e);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json array into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueArray(VocShaper* shaper, TRI_shape_value_t* dst,
                                TRI_json_t const* json, size_t level,
                                bool create) {
  TRI_shape_sid_t* sids;
  TRI_shape_aid_t* aids;
  TRI_shape_size_t* offsetsF;
  TRI_shape_size_t* offsetsV;
  TRI_shape_size_t offset;

  char* ptr;

  // sanity checks
  TRI_ASSERT(json->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(TRI_LengthVector(&json->_value._objects) % 2 == 0);

  // number of attributes
  size_t n = TRI_LengthVector(&json->_value._objects) / 2;

  // convert into TRI_shape_value_t array
  TRI_shape_value_t* values = static_cast<TRI_shape_value_t*>(
      TRI_Allocate(shaper->memoryZone(), n * sizeof(TRI_shape_value_t), true));

  if (values == nullptr) {
    return false;
  }

  // helper function to free shape values
  auto freeShapeValues = [](TRI_memory_zone_t* zone, TRI_shape_value_t* values,
                            TRI_shape_value_t* end) {
    TRI_shape_value_t* p = values;
    TRI_shape_value_t* e = end;

    for (; p < e; ++p) {
      if (p->_value != nullptr) {
        TRI_Free(zone, p->_value);
      }
    }

    TRI_Free(zone, values);
  };

  uint64_t total = 0;
  size_t f = 0;
  size_t v = 0;

  TRI_shape_value_t* p = values;

  for (size_t i = 0; i < n; ++i, ++p) {
    TRI_json_t const* key = static_cast<TRI_json_t const*>(
        TRI_AtVector(&json->_value._objects, 2 * i));
    TRI_ASSERT(key != nullptr);
    TRI_ASSERT(key->_type == TRI_JSON_STRING);

    char const* k = key->_value._string.data;

    if (k == nullptr || key->_value._string.length == 1) {
      // empty attribute name
      p--;
      continue;
    }

    if (*k == '_' && level == 0) {
      // on top level, strip reserved attributes before shaping
      if (strcmp(k, "_key") == 0 || strcmp(k, "_rev") == 0 ||
          strcmp(k, "_id") == 0 || strcmp(k, "_from") == 0 ||
          strcmp(k, "_to") == 0) {
        // found a reserved attribute - discard it
        --p;
        continue;
      }
    }

    // first find an identifier for the name
    p->_aid = shaper->findOrCreateAttributeByName(k);

    // convert value
    bool ok;
    if (p->_aid == 0) {
      ok = false;
    } else {
      auto val = static_cast<TRI_json_t const*>(
          TRI_AtVector(&json->_value._objects, 2 * i + 1));
      TRI_ASSERT(val != nullptr);

      ok = FillShapeValueJson(shaper, p, val, level + 1, create);
    }

    if (!ok) {
      freeShapeValues(shaper->memoryZone(), values, p);
      return false;
    }

    total += p->_size;

    // count fixed and variable sized values
    if (p->_fixedSized) {
      ++f;
    } else {
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
         (unsigned int)n, (unsigned int)f, (unsigned int)v);
  PrintShapeValues(values, n);
  printf("\n");
#endif

  // generate shape structure
  size_t byteSize = sizeof(TRI_array_shape_t) + n * sizeof(TRI_shape_sid_t) +
                    n * sizeof(TRI_shape_aid_t) +
                    (f + 1) * sizeof(TRI_shape_size_t);

  TRI_array_shape_t* a = reinterpret_cast<TRI_array_shape_t*>(
      ptr = static_cast<char*>(
          TRI_Allocate(shaper->memoryZone(), byteSize, true)));

  if (ptr == nullptr) {
    freeShapeValues(shaper->memoryZone(), values, values + n);
    return false;
  }

  a->base._type = TRI_SHAPE_ARRAY;
  a->base._size = byteSize;
  a->base._dataSize = (v == 0) ? total : TRI_SHAPE_SIZE_VARIABLE;

  a->_fixedEntries = f;
  a->_variableEntries = v;

  ptr += sizeof(TRI_array_shape_t);

  // array of shape identifiers
  sids = (TRI_shape_sid_t*)ptr;
  ptr += n * sizeof(TRI_shape_sid_t);

  // array of attribute identifiers
  aids = (TRI_shape_aid_t*)ptr;
  ptr += n * sizeof(TRI_shape_aid_t);

  // array of offsets for fixed part (within the shape)
  offset = (v + 1) * sizeof(TRI_shape_size_t);
  offsetsF = (TRI_shape_size_t*)ptr;

  // fill destination (except sid)
  dst->_type = TRI_SHAPE_ARRAY;

  dst->_fixedSized = true;
  dst->_size = total;
  dst->_value = (ptr = static_cast<char*>(
                     TRI_Allocate(shaper->memoryZone(), dst->_size, true)));

  if (ptr == nullptr) {
    freeShapeValues(shaper->memoryZone(), values, values + n);
    TRI_Free(shaper->memoryZone(), a);
    return false;
  }

  // array of offsets for variable part (within the value)
  offsetsV = (TRI_shape_size_t*)ptr;
  ptr += (v + 1) * sizeof(TRI_shape_size_t);

  // and fill in attributes
  TRI_shape_value_t* const e = values + n;

  for (p = values; p < e; ++p) {
    *aids++ = p->_aid;
    *sids++ = p->_sid;

    TRI_ASSERT(p->_value != nullptr || p->_size == 0);
    if (p->_value != nullptr) {
      memcpy(ptr, p->_value, (size_t)p->_size);
    }
    ptr += p->_size;

    dst->_fixedSized &= p->_fixedSized;

    if (p->_fixedSized) {
      *offsetsF++ = offset;
      offset += p->_size;
      *offsetsF = offset;
    } else {
      *offsetsV++ = offset;
      offset += p->_size;
      *offsetsV = offset;
    }
  }

  // free TRI_shape_value_t array
  freeShapeValues(shaper->memoryZone(), values, e);

  // lookup this shape
  TRI_shape_t const* found = shaper->findShape(&a->base, create);

  if (found == nullptr) {
    TRI_Free(shaper->memoryZone(), dst->_value);
    TRI_Free(shaper->memoryZone(), a);
    return false;
  }

  // and finally add the sid
  dst->_sid = found->_sid;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueJson(VocShaper* shaper, TRI_shape_value_t* dst,
                               TRI_json_t const* json, size_t level,
                               bool create) {
  TRI_ASSERT(json != nullptr);

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

static inline int JsonShapeDataNull(TRI_json_t* dst) {
  TRI_InitNullJson(dst);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data boolean blob into a json object
////////////////////////////////////////////////////////////////////////////////

static inline int JsonShapeDataBoolean(TRI_json_t* dst, char const* data) {
  bool v = (*(TRI_shape_boolean_t const*)data) != 0;
  TRI_InitBooleanJson(dst, v);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data number blob into a json object
////////////////////////////////////////////////////////////////////////////////

static inline int JsonShapeDataNumber(TRI_json_t* dst, char const* data) {
  TRI_shape_number_t v = *(TRI_shape_number_t const*)(void const*)data;
  TRI_InitNumberJson(dst, v);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data short string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static inline int JsonShapeDataShortString(VocShaper* shaper, TRI_json_t* dst,
                                           char const* data) {
  TRI_shape_length_short_string_t l =
      *(TRI_shape_length_short_string_t const*)data;
  data += sizeof(TRI_shape_length_short_string_t);

  return TRI_InitStringCopyJson(shaper->memoryZone(), dst, data,
                                static_cast<size_t>(l - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data long string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static inline int JsonShapeDataLongString(VocShaper* shaper, TRI_json_t* dst,
                                          char const* data) {
  TRI_shape_length_long_string_t l =
      *(TRI_shape_length_long_string_t const*)data;
  data += sizeof(TRI_shape_length_long_string_t);

  return TRI_InitStringCopyJson(shaper->memoryZone(), dst, data,
                                static_cast<size_t>(l - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data array blob into a json object
////////////////////////////////////////////////////////////////////////////////

static int JsonShapeDataArray(VocShaper* shaper, TRI_shape_t const* shape,
                              TRI_json_t* dst, char const* data,
                              uint64_t size) {
  TRI_array_shape_t const* s = (TRI_array_shape_t const*)shape;
  TRI_shape_size_t const f = s->_fixedEntries;
  TRI_shape_size_t const v = s->_variableEntries;
  TRI_shape_size_t const n = f + v;

  // create an array with the appropriate size
  TRI_InitObjectJson(shaper->memoryZone(), dst, static_cast<size_t>(n));

  int res =
      TRI_ReserveVector(&dst->_value._objects, static_cast<size_t>(n * 2));

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  char const* qtr = (char const*)shape;
  qtr += sizeof(TRI_array_shape_t);

  TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*)qtr;
  qtr += n * sizeof(TRI_shape_sid_t);

  TRI_shape_aid_t const* aids = (TRI_shape_aid_t const*)qtr;
  qtr += n * sizeof(TRI_shape_aid_t);

  TRI_shape_size_t const* offsetsF = (TRI_shape_size_t const*)qtr;

  shape_cache_t shapeCache;
  shapeCache._sid = 0;
  shapeCache._shape = nullptr;

  for (TRI_shape_size_t i = 0; i < f; ++i, ++sids, ++aids, ++offsetsF) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_aid_t aid = *aids;
    TRI_shape_t const* subshape;

    // use last sid if in cache
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    } else {
      shapeCache._shape = subshape = shaper->lookupShapeId(sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    char const* name = shaper->lookupAttributeId(aid);

    if (name == nullptr) {
      LOG(WARN) << "cannot find attribute #" << aid;
      continue;
    }

    TRI_shape_size_t offset = *offsetsF;
    // calculate target address for key
    auto next = static_cast<TRI_json_t*>(TRI_NextVector(&dst->_value._objects));
    res =
        TRI_InitStringCopyJson(shaper->memoryZone(), next, name, strlen(name));

    if (res != TRI_ERROR_NO_ERROR) {
      // return the element
      TRI_ReturnVector(&dst->_value._objects);
      return res;
    }

    // save position of key
    auto key = next;

    // calculate target address for value
    next = static_cast<TRI_json_t*>(TRI_NextVector(&dst->_value._objects));
    res = JsonShapeData(shaper, subshape, next, data + offset,
                        offsetsF[1] - offset);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_DestroyJson(shaper->memoryZone(), key);
      TRI_ReturnVector(&dst->_value._objects);
      TRI_ReturnVector(&dst->_value._objects);
      return res;
    }
  }

  TRI_shape_size_t const* offsetsV = (TRI_shape_size_t const*)data;

  for (TRI_shape_size_t i = 0; i < v; ++i, ++sids, ++aids, ++offsetsV) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_aid_t aid = *aids;
    TRI_shape_t const* subshape;

    // use last sid if in cache
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    } else {
      shapeCache._shape = subshape = shaper->lookupShapeId(sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    char const* name = shaper->lookupAttributeId(aid);

    if (name == nullptr) {
      LOG(WARN) << "cannot find attribute #" << aid;
      continue;
    }

    TRI_shape_size_t offset = *offsetsV;

    // calculate target address for key
    auto next = static_cast<TRI_json_t*>(TRI_NextVector(&dst->_value._objects));
    res =
        TRI_InitStringCopyJson(shaper->memoryZone(), next, name, strlen(name));

    if (res != TRI_ERROR_NO_ERROR) {
      // return the element
      TRI_ReturnVector(&dst->_value._objects);
      return res;
    }

    // save position of key
    auto key = next;

    // calculate target address for value
    next = static_cast<TRI_json_t*>(TRI_NextVector(&dst->_value._objects));
    res = JsonShapeData(shaper, subshape, next, data + offset,
                        offsetsV[1] - offset);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_DestroyJson(shaper->memoryZone(), key);
      TRI_ReturnVector(&dst->_value._objects);
      TRI_ReturnVector(&dst->_value._objects);
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static int JsonShapeDataList(VocShaper* shaper, TRI_shape_t const* shape,
                             TRI_json_t* dst, char const* data, uint64_t size) {
  char const* ptr = data;
  TRI_shape_length_list_t const l = *(TRI_shape_length_list_t const*)ptr;

  // create an array with the appropriate size
  TRI_InitArrayJson(shaper->memoryZone(), dst, static_cast<size_t>(l));

  int res = TRI_ReserveVector(&dst->_value._objects, static_cast<size_t>(l));

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  ptr += sizeof(TRI_shape_length_list_t);
  TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*)ptr;

  ptr += l * sizeof(TRI_shape_sid_t);
  TRI_shape_size_t const* offsets = (TRI_shape_size_t const*)ptr;

  shape_cache_t shapeCache;
  shapeCache._sid = 0;
  shapeCache._shape = nullptr;

  for (TRI_shape_length_list_t i = 0; i < l; ++i, ++sids, ++offsets) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_t const* subshape;

    TRI_shape_size_t offset = *offsets;

    // use last sid if in cache
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    } else {
      shapeCache._shape = subshape = shaper->lookupShapeId(sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    // calculate target address for key
    auto next = static_cast<TRI_json_t*>(TRI_NextVector(&dst->_value._objects));
    res = JsonShapeData(shaper, subshape, next, data + offset,
                        offsets[1] - offset);

    if (res != TRI_ERROR_NO_ERROR) {
      // return the element
      TRI_ReturnVector(&dst->_value._objects);
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static int JsonShapeDataHomogeneousList(VocShaper* shaper,
                                        TRI_shape_t const* shape,
                                        TRI_json_t* dst, char const* data,
                                        uint64_t size) {
  TRI_homogeneous_list_shape_t const* s =
      (TRI_homogeneous_list_shape_t const*)shape;
  TRI_shape_sid_t const sid = s->_sidEntry;

  TRI_shape_t const* subshape = shaper->lookupShapeId(sid);

  if (subshape == nullptr) {
    LOG(WARN) << "cannot find shape #" << sid;

    return TRI_ERROR_INTERNAL;
  }

  char const* ptr = data;
  TRI_shape_length_list_t const l = *(TRI_shape_length_list_t const*)ptr;

  // create an array with the appropriate size
  TRI_InitArrayJson(shaper->memoryZone(), dst, static_cast<size_t>(l));

  int res = TRI_ReserveVector(&dst->_value._objects, static_cast<size_t>(l));

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  ptr += sizeof(TRI_shape_length_list_t);
  TRI_shape_size_t const* offsets = (TRI_shape_size_t const*)ptr;

  for (TRI_shape_length_list_t i = 0; i < l; ++i, ++offsets) {
    TRI_shape_size_t offset = *offsets;

    // calculate target address for key
    auto next = static_cast<TRI_json_t*>(TRI_NextVector(&dst->_value._objects));
    res = JsonShapeData(shaper, subshape, next, data + offset,
                        offsets[1] - offset);

    if (res != TRI_ERROR_NO_ERROR) {
      // return the element
      TRI_ReturnVector(&dst->_value._objects);
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous sized list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static int JsonShapeDataHomogeneousSizedList(VocShaper* shaper,
                                             TRI_shape_t const* shape,
                                             TRI_json_t* dst, char const* data,
                                             uint64_t size) {
  TRI_homogeneous_sized_list_shape_t const* s =
      (TRI_homogeneous_sized_list_shape_t const*)shape;
  TRI_shape_sid_t const sid = s->_sidEntry;

  TRI_shape_t const* subshape = shaper->lookupShapeId(sid);

  if (subshape == nullptr) {
    LOG(WARN) << "cannot find shape #" << sid;

    return TRI_ERROR_INTERNAL;
  }

  char const* ptr = data;
  TRI_shape_length_list_t const l = *(TRI_shape_length_list_t const*)ptr;

  // create an array with the appropriate size
  TRI_InitArrayJson(shaper->memoryZone(), dst, static_cast<size_t>(l));

  int res = TRI_ReserveVector(&dst->_value._objects, static_cast<size_t>(l));

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_shape_size_t const length = s->_sizeEntry;
  TRI_shape_size_t offset = sizeof(TRI_shape_length_list_t);

  for (TRI_shape_length_list_t i = 0; i < l; ++i, offset += length) {
    // calculate target address for key
    auto next = static_cast<TRI_json_t*>(TRI_NextVector(&dst->_value._objects));
    res = JsonShapeData(shaper, subshape, next, data + offset, length);

    if (res != TRI_ERROR_NO_ERROR) {
      // return the element
      TRI_ReturnVector(&dst->_value._objects);
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data blob into a json object
////////////////////////////////////////////////////////////////////////////////

static int JsonShapeData(VocShaper* shaper, TRI_shape_t const* shape,
                         TRI_json_t* dst, char const* data, uint64_t size) {
  if (shape == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  switch (shape->_type) {
    case TRI_SHAPE_NULL:
      return JsonShapeDataNull(dst);

    case TRI_SHAPE_BOOLEAN:
      return JsonShapeDataBoolean(dst, data);

    case TRI_SHAPE_NUMBER:
      return JsonShapeDataNumber(dst, data);

    case TRI_SHAPE_SHORT_STRING:
      return JsonShapeDataShortString(shaper, dst, data);

    case TRI_SHAPE_LONG_STRING:
      return JsonShapeDataLongString(shaper, dst, data);

    case TRI_SHAPE_ARRAY:
      return JsonShapeDataArray(shaper, shape, dst, data, size);

    case TRI_SHAPE_LIST:
      return JsonShapeDataList(shaper, shape, dst, data, size);

    case TRI_SHAPE_HOMOGENEOUS_LIST:
      return JsonShapeDataHomogeneousList(shaper, shape, dst, data, size);

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      return JsonShapeDataHomogeneousSizedList(shaper, shape, dst, data, size);
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data null blob into a json object
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static bool StringifyJsonShapeDataNull(T* shaper, TRI_string_buffer_t* buffer,
                                       TRI_shape_t const* shape,
                                       char const* data, uint64_t size) {
  int res = TRI_AppendString2StringBuffer(buffer, "null", 4);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a data boolean blob into a json object
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static bool StringifyJsonShapeDataBoolean(T* shaper,
                                          TRI_string_buffer_t* buffer,
                                          TRI_shape_t const* shape,
                                          char const* data, uint64_t size) {
  int res;

  bool v = (*(TRI_shape_boolean_t const*)data) != 0;

  if (v) {
    res = TRI_AppendString2StringBuffer(buffer, "true", 4);
  } else {
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

template <typename T>
static bool StringifyJsonShapeDataNumber(T* shaper, TRI_string_buffer_t* buffer,
                                         TRI_shape_t const* shape,
                                         char const* data, uint64_t size) {
  int res;

  TRI_shape_number_t v = *(TRI_shape_number_t const*)(void const*)data;
  // check for special values

  // yes, this is intentional
  if (v != v) {
    // NaN
    res = TRI_AppendString2StringBuffer(buffer, "null", 4);
  } else if (v == HUGE_VAL) {
    // +inf
    res = TRI_AppendString2StringBuffer(buffer, "null", 4);
  } else if (v == -HUGE_VAL) {
    // -inf
    res = TRI_AppendString2StringBuffer(buffer, "null", 4);
  } else {
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

template <typename T>
static bool StringifyJsonShapeDataShortString(T* shaper,
                                              TRI_string_buffer_t* buffer,
                                              TRI_shape_t const* shape,
                                              char const* data, uint64_t size) {
  // note: length includes the NULL byte
  TRI_shape_length_short_string_t const length =
      *((TRI_shape_length_short_string_t const*)data);
  data += sizeof(TRI_shape_length_short_string_t);
  TRI_ASSERT(length + sizeof(TRI_shape_length_short_string_t) <= size);

  int res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  res = TRI_AppendJsonEncodedStringStringBuffer(
      buffer, data, static_cast<size_t>(length) - 1, true);

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

template <typename T>
static bool StringifyJsonShapeDataLongString(T* shaper,
                                             TRI_string_buffer_t* buffer,
                                             TRI_shape_t const* shape,
                                             char const* data, uint64_t size) {
  // note: length includes the NULL byte
  TRI_shape_length_long_string_t const length =
      *((TRI_shape_length_long_string_t const*)data);
  data += sizeof(TRI_shape_length_long_string_t);
  TRI_ASSERT(length + sizeof(TRI_shape_length_long_string_t) <= size);

  int res = TRI_AppendCharStringBuffer(buffer, '"');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  res = TRI_AppendJsonEncodedStringStringBuffer(
      buffer, data, static_cast<size_t>(length) - 1, true);

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

template <typename T>
static bool StringifyJsonShapeDataArray(T* shaper, TRI_string_buffer_t* buffer,
                                        TRI_shape_t const* shape,
                                        char const* data, uint64_t size,
                                        bool braces, uint64_t* num) {
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

  s = (TRI_array_shape_t const*)shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;
  first = true;

  if (num != nullptr) {
    *num = n;
  }

  qtr = (char const*)shape;

  if (braces) {
    res = TRI_AppendCharStringBuffer(buffer, '{');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }
  }

  qtr += sizeof(TRI_array_shape_t);

  sids = (TRI_shape_sid_t const*)qtr;
  qtr += n * sizeof(TRI_shape_sid_t);

  aids = (TRI_shape_aid_t const*)qtr;
  qtr += n * sizeof(TRI_shape_aid_t);

  offsetsF = (TRI_shape_size_t const*)qtr;

  shapeCache._sid = 0;
  shapeCache._shape = nullptr;

  for (i = 0; i < f; ++i, ++sids, ++aids, ++offsetsF) {
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
    } else {
      shapeCache._shape = subshape = shaper->lookupShapeId(sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    name = shaper->lookupAttributeId(aid);

    if (name == nullptr) {
      LOG(WARN) << "cannot find attribute #" << aid;
      continue;
    }

    if (first) {
      first = false;
    } else {
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

    ok = StringifyJsonShapeData<T>(shaper, buffer, subshape, data + offset,
                                   offsetsF[1] - offset);

    if (!ok) {
      LOG(WARN) << "cannot decode element for shape #" << sid;
      continue;
    }
  }

  offsetsV = (TRI_shape_size_t const*)data;

  for (i = 0; i < v; ++i, ++sids, ++aids, ++offsetsV) {
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
    } else {
      shapeCache._shape = subshape = shaper->lookupShapeId(sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    name = shaper->lookupAttributeId(aid);

    if (name == nullptr) {
      LOG(WARN) << "cannot find attribute #" << aid;
      continue;
    }

    if (first) {
      first = false;
    } else {
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

    ok = StringifyJsonShapeData<T>(shaper, buffer, subshape, data + offset,
                                   offsetsV[1] - offset);

    if (!ok) {
      LOG(WARN) << "cannot decode element for shape #" << sid;
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

template <typename T>
static bool StringifyJsonShapeDataList(T* shaper, TRI_string_buffer_t* buffer,
                                       TRI_shape_t const* shape,
                                       char const* data, uint64_t size) {
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
  l = *(TRI_shape_length_list_t const*)ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  sids = (TRI_shape_sid_t const*)ptr;

  ptr += l * sizeof(TRI_shape_sid_t);
  offsets = (TRI_shape_size_t const*)ptr;

  res = TRI_AppendCharStringBuffer(buffer, '[');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  shapeCache._sid = 0;
  shapeCache._shape = nullptr;

  for (i = 0; i < l; ++i, ++sids, ++offsets) {
    TRI_shape_sid_t sid;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    bool ok;

    sid = *sids;
    offset = *offsets;

    // use last sid if in cache
    if (sid == shapeCache._sid && shapeCache._sid > 0) {
      subshape = shapeCache._shape;
    } else {
      shapeCache._shape = subshape = shaper->lookupShapeId(sid);
      shapeCache._sid = sid;
    }

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    if (first) {
      first = false;
    } else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    ok = StringifyJsonShapeData<T>(shaper, buffer, subshape, data + offset,
                                   offsets[1] - offset);

    if (!ok) {
      LOG(WARN) << "cannot decode element for shape #" << sid;
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

template <typename T>
static bool StringifyJsonShapeDataHomogeneousList(T* shaper,
                                                  TRI_string_buffer_t* buffer,
                                                  TRI_shape_t const* shape,
                                                  char const* data,
                                                  uint64_t size) {
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_size_t const* offsets;
  int res;

  TRI_homogeneous_list_shape_t const* s =
      (TRI_homogeneous_list_shape_t const*)shape;
  TRI_shape_sid_t sid = s->_sidEntry;

  TRI_shape_t const* subshape = shaper->lookupShapeId(sid);

  if (subshape == nullptr) {
    LOG(WARN) << "cannot find shape #" << sid;

    return false;
  }

  char const* ptr = data;
  bool first = true;

  l = *(TRI_shape_length_list_t const*)ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  offsets = (TRI_shape_size_t const*)ptr;

  res = TRI_AppendCharStringBuffer(buffer, '[');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  for (i = 0; i < l; ++i, ++offsets) {
    TRI_shape_size_t offset;
    bool ok;

    offset = *offsets;

    if (first) {
      first = false;
    } else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    ok = StringifyJsonShapeData<T>(shaper, buffer, subshape, data + offset,
                                   offsets[1] - offset);

    if (!ok) {
      LOG(WARN) << "cannot decode element for shape #" << sid;
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

template <typename T>
static bool StringifyJsonShapeDataHomogeneousSizedList(
    T* shaper, TRI_string_buffer_t* buffer, TRI_shape_t const* shape,
    char const* data, uint64_t size) {
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_size_t length;
  TRI_shape_size_t offset;
  bool first;
  char const* ptr;
  int res;

  TRI_homogeneous_sized_list_shape_t const* s =
      (TRI_homogeneous_sized_list_shape_t const*)shape;
  TRI_shape_sid_t sid = s->_sidEntry;
  TRI_shape_t const* subshape = shaper->lookupShapeId(sid);

  if (subshape == nullptr) {
    LOG(WARN) << "cannot find shape #" << sid;

    return false;
  }

  ptr = data;
  first = true;

  length = s->_sizeEntry;
  offset = sizeof(TRI_shape_length_list_t);

  l = *(TRI_shape_length_list_t const*)ptr;

  res = TRI_AppendCharStringBuffer(buffer, '[');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  for (i = 0; i < l; ++i, offset += length) {
    bool ok;

    if (first) {
      first = false;
    } else {
      res = TRI_AppendCharStringBuffer(buffer, ',');

      if (res != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    ok = StringifyJsonShapeData<T>(shaper, buffer, subshape, data + offset,
                                   length);

    if (!ok) {
      LOG(WARN) << "cannot decode element for shape #" << sid;
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

template <typename T>
static bool StringifyJsonShapeData(T* shaper, TRI_string_buffer_t* buffer,
                                   TRI_shape_t const* shape, char const* data,
                                   uint64_t size) {
  if (shape == nullptr) {
    return false;
  }

  switch (shape->_type) {
    case TRI_SHAPE_NULL:
      return StringifyJsonShapeDataNull<T>(shaper, buffer, shape, data, size);

    case TRI_SHAPE_BOOLEAN:
      return StringifyJsonShapeDataBoolean<T>(shaper, buffer, shape, data,
                                              size);

    case TRI_SHAPE_NUMBER:
      return StringifyJsonShapeDataNumber<T>(shaper, buffer, shape, data, size);

    case TRI_SHAPE_SHORT_STRING:
      return StringifyJsonShapeDataShortString<T>(shaper, buffer, shape, data,
                                                  size);

    case TRI_SHAPE_LONG_STRING:
      return StringifyJsonShapeDataLongString<T>(shaper, buffer, shape, data,
                                                 size);

    case TRI_SHAPE_ARRAY:
      return StringifyJsonShapeDataArray<T>(shaper, buffer, shape, data, size,
                                            true, nullptr);

    case TRI_SHAPE_LIST:
      return StringifyJsonShapeDataList<T>(shaper, buffer, shape, data, size);

    case TRI_SHAPE_HOMOGENEOUS_LIST:
      return StringifyJsonShapeDataHomogeneousList<T>(shaper, buffer, shape,
                                                      data, size);

    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
      return StringifyJsonShapeDataHomogeneousSizedList<T>(shaper, buffer,
                                                           shape, data, size);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShapedJson(TRI_memory_zone_t* zone, TRI_shaped_json_t* shaped) {
  TRI_DestroyBlob(zone, &shaped->_data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapedJson(TRI_memory_zone_t* zone, TRI_shaped_json_t* shaped) {
  TRI_DestroyShapedJson(zone, shaped);
  TRI_Free(zone, shaped);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a list of TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

void TRI_SortShapeValues(TRI_shape_value_t* values, size_t n) {
  qsort(values, n, sizeof(TRI_shape_value_t), SortShapeValuesFunc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VelocyPack object into a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonVelocyPack(VocShaper* shaper,
                                            VPackSlice const& slice,
                                            bool create) {
  std::unique_ptr<TRI_json_t> json(
      arangodb::basics::VelocyPackHelper::velocyPackToJson(slice));
  return TRI_ShapedJsonJson(shaper, json.get(), create);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonJson(VocShaper* shaper, TRI_json_t const* json,
                                      bool create) {
  TRI_ASSERT(json != nullptr);

  TRI_shape_value_t dst;

  dst._value = nullptr;
  bool ok = FillShapeValueJson(shaper, &dst, json, 0, create);

  if (!ok) {
    return nullptr;
  }

#ifdef DEBUG_JSON_SHAPER
  printf("shape\n-----\n");
  TRI_PrintShape(shaper, shaper->lookupShapeId(dst._sid), 0);
  printf("\n");
#endif

  // no need to prefill shaped with 0's as all attributes are set directly
  // afterwards
  TRI_shaped_json_t* shaped = static_cast<TRI_shaped_json_t*>(
      TRI_Allocate(shaper->memoryZone(), sizeof(TRI_shaped_json_t), false));

  if (shaped == nullptr) {
    if (dst._value != nullptr) {
      TRI_Free(shaper->memoryZone(), dst._value);
    }
    return nullptr;
  }

  shaped->_sid = dst._sid;
  shaped->_data.length = (uint32_t)dst._size;
  shaped->_data.data = dst._value;

  return shaped;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a shaped json object into a json object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonShapedJson(VocShaper* shaper,
                               TRI_shaped_json_t const* shaped) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(shaped != nullptr);
#endif

  TRI_shape_t const* shape = shaper->lookupShapeId(shaped->_sid);

  if (shape == nullptr) {
    LOG(WARN) << "cannot find shape #" << shaped->_sid;
    return nullptr;
  }

  auto dst = TRI_CreateNullJson(shaper->memoryZone());

  if (dst == nullptr) {
    // out of memory
    return nullptr;
  }

  int res = JsonShapeData(shaper, shape, dst, shaped->_data.data,
                          shaped->_data.length);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(shaper->memoryZone(), dst);
    return nullptr;
  }

  return dst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer, without the outer braces
/// this can only be used to stringify shapes of type array
////////////////////////////////////////////////////////////////////////////////

template <typename T>
bool TRI_StringifyArrayShapedJson(T* shaper, TRI_string_buffer_t* buffer,
                                  TRI_shaped_json_t const* shaped,
                                  bool prepend) {
  TRI_shape_t const* shape = shaper->lookupShapeId(shaped->_sid);

  if (shape == nullptr || shape->_type != TRI_SHAPE_ARRAY) {
    return false;
  }

  if (prepend) {
    TRI_array_shape_t const* s = (TRI_array_shape_t const*)shape;
    if (s->_fixedEntries + s->_variableEntries > 0) {
      TRI_AppendCharStringBuffer(buffer, ',');
    }
  }

  uint64_t num;
  return StringifyJsonShapeDataArray(shaper, buffer, shape, shaped->_data.data,
                                     shaped->_data.length, false, &num);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyAugmentedShapedJson(VocShaper* shaper,
                                      struct TRI_string_buffer_s* buffer,
                                      TRI_shaped_json_t const* shaped,
                                      TRI_json_t const* augment) {
  TRI_shape_t const* shape = shaper->lookupShapeId(shaped->_sid);

  if (shape == nullptr) {
    return false;
  }

  if (augment == nullptr || augment->_type != TRI_JSON_OBJECT ||
      shape->_type != TRI_SHAPE_ARRAY) {
    return StringifyJsonShapeData<VocShaper>(
        shaper, buffer, shape, shaped->_data.data, shaped->_data.length);
  }

  int res = TRI_AppendCharStringBuffer(buffer, '{');

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  uint64_t num;
  bool ok = StringifyJsonShapeDataArray<VocShaper>(
      shaper, buffer, shape, shaped->_data.data, shaped->_data.length, false,
      &num);

  if (0 < num) {
    res = TRI_AppendCharStringBuffer(buffer, ',');

    if (res != TRI_ERROR_NO_ERROR) {
      return false;
    }
  }

  if (!ok) {
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

size_t TRI_LengthListShapedJson(TRI_list_shape_t const* shape,
                                TRI_shaped_json_t const* json) {
  return *(TRI_shape_length_list_t*)json->_data.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtListShapedJson(TRI_list_shape_t const* shape,
                          TRI_shaped_json_t const* json, size_t position,
                          TRI_shaped_json_t* result) {
  char const* ptr = json->_data.data;
  TRI_shape_length_list_t n = *(TRI_shape_length_list_t*)ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  TRI_shape_sid_t* sids = (TRI_shape_sid_t*)ptr;

  ptr += n * sizeof(TRI_shape_sid_t);
  TRI_shape_size_t* offsets = (TRI_shape_size_t*)ptr;

  result->_sid = sids[position];
  result->_data.data = ((char*)json->_data.data) + offsets[position];
  result->_data.length = (uint32_t)(offsets[position + 1] - offsets[position]);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a homogeneous list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthHomogeneousListShapedJson(
    TRI_homogeneous_list_shape_t const* shape, TRI_shaped_json_t const* json) {
  return *(TRI_shape_length_list_t*)json->_data.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a homogeneous list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtHomogeneousListShapedJson(TRI_homogeneous_list_shape_t const* shape,
                                     TRI_shaped_json_t const* json,
                                     size_t position,
                                     TRI_shaped_json_t* result) {
  char const* ptr = json->_data.data;
  TRI_shape_length_list_t n = *(TRI_shape_length_list_t*)ptr;

  if (n <= position) {
    return false;
  }

  ptr += sizeof(TRI_shape_length_list_t);
  TRI_shape_size_t* offsets = (TRI_shape_size_t*)ptr;

  result->_sid = shape->_sidEntry;
  result->_data.data = ((char*)json->_data.data) + offsets[position];
  result->_data.length = (uint32_t)(offsets[position + 1] - offsets[position]);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a homogeneous sized list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthHomogeneousSizedListShapedJson(
    TRI_homogeneous_sized_list_shape_t const* shape,
    TRI_shaped_json_t const* json) {
  return *(TRI_shape_length_list_t*)json->_data.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a homogeneous sized list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtHomogeneousSizedListShapedJson(
    TRI_homogeneous_sized_list_shape_t const* shape,
    TRI_shaped_json_t const* json, size_t position, TRI_shaped_json_t* result) {
  char* ptr = json->_data.data;
  TRI_shape_length_list_t n = *(TRI_shape_length_list_t*)ptr;

  if (n <= position) {
    return false;
  }

  ptr += sizeof(TRI_shape_length_list_t);

  result->_sid = shape->_sidEntry;
  result->_data.data = ptr + (shape->_sizeEntry * position);
  result->_data.length = (uint32_t)shape->_sizeEntry;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string value encoded in a shaped json
/// this will return the pointer to the string and the string length in the
/// variables passed by reference
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringValueShapedJson(TRI_shape_t const* shape, char const* data,
                               char** value, size_t* length) {
  if (shape->_type == TRI_SHAPE_SHORT_STRING) {
    TRI_shape_length_short_string_t l;

    l = *(TRI_shape_length_short_string_t const*)data;
    data += sizeof(TRI_shape_length_short_string_t);
    *length = l - 1;
    *value = (char*)data;

    return true;
  } else if (shape->_type == TRI_SHAPE_LONG_STRING) {
    TRI_shape_length_long_string_t l;

    l = *(TRI_shape_length_long_string_t const*)data;
    data += sizeof(TRI_shape_length_long_string_t);
    *length = l - 1;
    *value = (char*)data;

    return true;
  }

  // no string type
  *value = nullptr;
  *length = 0;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over a shaped json object, using a callback function
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateShapeDataArray(VocShaper* shaper, TRI_shape_t const* shape,
                               char const* data,
                               bool (*filter)(VocShaper*, TRI_shape_t const*,
                                              char const*, char const*,
                                              uint64_t, void*),
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

  TRI_ASSERT(shape->_type == TRI_SHAPE_ARRAY);

  s = (TRI_array_shape_t const*)shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;

  qtr = (char const*)shape;
  qtr += sizeof(TRI_array_shape_t);

  sids = (TRI_shape_sid_t const*)qtr;
  qtr += n * sizeof(TRI_shape_sid_t);

  aids = (TRI_shape_aid_t const*)qtr;
  qtr += n * sizeof(TRI_shape_aid_t);

  offsetsF = (TRI_shape_size_t const*)qtr;

  cachedSid = 0;
  cachedShape = nullptr;

  for (i = 0; i < f; ++i, ++sids, ++aids, ++offsetsF) {
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
    } else {
      cachedShape = subshape = shaper->lookupShapeId(sid);
      cachedSid = sid;
    }

    name = shaper->lookupAttributeId(aid);

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    if (name == nullptr) {
      LOG(WARN) << "cannot find attribute #" << aid;
      continue;
    }

    if (!filter(shaper, subshape, name, data + offset, offsetsF[1] - offset,
                ptr)) {
      return;
    }
  }

  offsetsV = (TRI_shape_size_t const*)data;

  for (i = 0; i < v; ++i, ++sids, ++aids, ++offsetsV) {
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
    } else {
      cachedShape = subshape = shaper->lookupShapeId(sid);
      cachedSid = sid;
    }

    name = shaper->lookupAttributeId(aid);

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      continue;
    }

    if (name == nullptr) {
      LOG(WARN) << "cannot find attribute #" << aid;
      continue;
    }

    if (!filter(shaper, subshape, name, data + offset, offsetsV[1] - offset,
                ptr)) {
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over a shaped json list, using a callback function
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateShapeDataList(VocShaper* shaper, TRI_shape_t const* shape,
                              char const* data,
                              bool (*filter)(VocShaper*, TRI_shape_t const*,
                                             char const*, uint64_t, void*),
                              void* ptr) {
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;

  if (shape->_type == TRI_SHAPE_LIST) {
    shape_cache_t shapeCache;

    char const* p = data;
    l = *(TRI_shape_length_list_t const*)p;

    p += sizeof(TRI_shape_length_list_t);
    TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*)p;

    p += l * sizeof(TRI_shape_sid_t);
    TRI_shape_size_t const* offsets = (TRI_shape_size_t const*)p;

    shapeCache._sid = 0;
    shapeCache._shape = nullptr;

    for (i = 0; i < l; ++i, ++sids, ++offsets) {
      TRI_shape_sid_t sid = *sids;
      TRI_shape_size_t offset = *offsets;

      // use last sid if in cache
      TRI_shape_t const* subshape;
      if (sid == shapeCache._sid && shapeCache._sid > 0) {
        subshape = shapeCache._shape;
      } else {
        shapeCache._shape = subshape = shaper->lookupShapeId(sid);
        shapeCache._sid = sid;
      }

      if (subshape == nullptr) {
        LOG(WARN) << "cannot find shape #" << sid;
        continue;
      }

      if (!filter(shaper, subshape, data + offset, offsets[1] - offset, ptr)) {
        return;
      }
    }
  } else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    TRI_homogeneous_list_shape_t const* s =
        (TRI_homogeneous_list_shape_t const*)shape;
    TRI_shape_sid_t sid = s->_sidEntry;

    TRI_shape_t const* subshape = shaper->lookupShapeId(sid);

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      return;
    }

    char const* p = data;
    l = *(TRI_shape_length_list_t const*)p;

    p += sizeof(TRI_shape_length_list_t);
    TRI_shape_size_t const* offsets = (TRI_shape_size_t const*)p;

    for (i = 0; i < l; ++i, ++offsets) {
      TRI_shape_size_t offset = *offsets;

      if (!filter(shaper, subshape, data + offset, offsets[1] - offset, ptr)) {
        return;
      }
    }
  } else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    TRI_homogeneous_sized_list_shape_t const* s =
        (TRI_homogeneous_sized_list_shape_t const*)shape;
    TRI_shape_sid_t sid = s->_sidEntry;

    TRI_shape_t const* subshape = shaper->lookupShapeId(sid);

    if (subshape == nullptr) {
      LOG(WARN) << "cannot find shape #" << sid;
      return;
    }

    char const* p = data;
    l = *(TRI_shape_length_list_t const*)p;

    TRI_shape_size_t length = s->_sizeEntry;
    TRI_shape_size_t offset = sizeof(TRI_shape_length_list_t);

    for (i = 0; i < l; ++i, offset += length) {
      if (!filter(shaper, subshape, data + offset, length, ptr)) {
        return;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a list of TRI_shape_value_t for debugging
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_JSON_SHAPER

void TRI_PrintShapeValues(TRI_shape_value_t* values, size_t n) {
  PrintShapeValues(values, n);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief explicit instantiation of the two functions
////////////////////////////////////////////////////////////////////////////////

template bool TRI_StringifyArrayShapedJson<VocShaper>(
    VocShaper*, struct TRI_string_buffer_s*, TRI_shaped_json_t const*, bool);
template bool TRI_StringifyArrayShapedJson<arangodb::basics::LegendReader>(
    arangodb::basics::LegendReader*, struct TRI_string_buffer_s*,
    TRI_shaped_json_t const*, bool);
