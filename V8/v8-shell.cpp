////////////////////////////////////////////////////////////////////////////////
/// @brief V8 shell functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-shell.h"

#define TRI_WITHIN_C
#include <Basics/conversions.h>
#include <Basics/csv.h>
#include <Basics/logging.h>
#include <Basics/string-buffer.h>
#include <Basics/strings.h>

#include "ShapedJson/shaped-json.h"
#undef TRI_WITHIN_C

#include <fstream>

#include "V8/v8-json.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static bool FillShapeValueJson (TRI_shaper_t* shaper,
                                TRI_shape_value_t* dst,
                                v8::Handle<v8::Value> json);

static v8::Handle<v8::Value> JsonShapeData (TRI_shaper_t* shaper,
                                            TRI_shape_t const* shape,
                                            char const* data,
                                            size_t size);

// -----------------------------------------------------------------------------
// --SECTION--                                              JAVASCRIPT KEY NAMES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ShellPrivate V8 Shell (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief "did" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> DidKey;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              CONVERSION FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ShellPrivate V8 Shell (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a null into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNull (TRI_shaper_t* shaper, TRI_shape_value_t* dst) {
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

static bool FillShapeValueBoolean (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::Boolean> json) {
  TRI_shape_boolean_t* ptr;

  dst->_type = TRI_SHAPE_BOOLEAN;
  dst->_sid = shaper->_sidBoolean;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_boolean_t);
  dst->_value = (char*)(ptr = (TRI_shape_boolean_t*) TRI_Allocate(dst->_size));

  *ptr = json->Value() ? 1 : 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a boolean into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueBoolean (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::BooleanObject> json) {
  TRI_shape_boolean_t* ptr;

  dst->_type = TRI_SHAPE_BOOLEAN;
  dst->_sid = shaper->_sidBoolean;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_boolean_t);
  dst->_value = (char*)(ptr = (TRI_shape_boolean_t*) TRI_Allocate(dst->_size));

  *ptr = json->BooleanValue() ? 1 : 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a number into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNumber (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::Number> json) {
  TRI_shape_number_t* ptr;

  dst->_type = TRI_SHAPE_NUMBER;
  dst->_sid = shaper->_sidNumber;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_number_t);
  dst->_value = (char*)(ptr = (TRI_shape_number_t*) TRI_Allocate(dst->_size));

  *ptr = json->Value();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a number into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueNumber (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::NumberObject> json) {
  TRI_shape_number_t* ptr;

  dst->_type = TRI_SHAPE_NUMBER;
  dst->_sid = shaper->_sidNumber;
  dst->_fixedSized = true;
  dst->_size = sizeof(TRI_shape_number_t);
  dst->_value = (char*)(ptr = (TRI_shape_number_t*) TRI_Allocate(dst->_size));

  *ptr = json->NumberValue();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a string into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueString (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::String> json) {
  char* ptr;

  v8::String::Utf8Value str(json);

  if (*str == 0) {
    dst->_type = TRI_SHAPE_SHORT_STRING;
    dst->_sid = shaper->_sidShortString;
    dst->_fixedSized = true;
    dst->_size = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
    dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

    * ((TRI_shape_length_short_string_t*) ptr) = 1;
    * (ptr + sizeof(TRI_shape_length_short_string_t)) = '\0';
  }
  else if (str.length() < TRI_SHAPE_SHORT_STRING_CUT) { // includes '\0'
    size_t size = str.length() + 1;

    dst->_type = TRI_SHAPE_SHORT_STRING;
    dst->_sid = shaper->_sidShortString;
    dst->_fixedSized = true;
    dst->_size = sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT;
    dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

    * ((TRI_shape_length_short_string_t*) ptr) = size;

    memcpy(ptr + sizeof(TRI_shape_length_short_string_t), *str, size);
  }
  else {
    size_t size = str.length() + 1;

    dst->_type = TRI_SHAPE_LONG_STRING;
    dst->_sid = shaper->_sidLongString;
    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_long_string_t) + size;
    dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

    * ((TRI_shape_length_long_string_t*) ptr) = size;

    memcpy(ptr + sizeof(TRI_shape_length_long_string_t), *str, size);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json list into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueList (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::Array> json) {
  size_t i;
  size_t n;
  size_t total;

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

  // check for special case "empty list"
  n = json->Length();

  if (n == 0) {
    dst->_type = TRI_SHAPE_LIST;
    dst->_sid = shaper->_sidList;

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t);
    dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

    * (TRI_shape_length_list_t*) ptr = 0;

    return true;
  }

  // convert into TRI_shape_value_t array
  p = (values = (TRI_shape_value_t*) TRI_Allocate(sizeof(TRI_shape_value_t) * n));
  memset(values, 0, sizeof(TRI_shape_value_t) * n);

  total = 0;
  e = values + n;

  for (i = 0;  i < n;  ++i, ++p) {
    v8::Local<v8::Value> el = json->Get(i);
    bool ok = FillShapeValueJson(shaper, p, el);

    if (! ok) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != 0) {
          TRI_Free(p->_value);
        }
      }

      TRI_Free(values);

      return false;
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
    TRI_homogeneous_sized_list_shape_t* shape;

    shape = (TRI_homogeneous_sized_list_shape_t*) TRI_Allocate(sizeof(TRI_homogeneous_sized_list_shape_t));

    shape->base._size = sizeof(TRI_homogeneous_sized_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_SIZED_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;
    shape->_sizeEntry = l;

    found = shaper->findShape(shaper, &shape->base);

    if (found == 0) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != 0) {
          TRI_Free(p->_value);
        }
      }

      TRI_Free(values);
      TRI_Free(shape);

      return false;
    }

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    dst->_fixedSized = false;
    dst->_size = sizeof(TRI_shape_length_list_t) + total;
    dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = n;
    ptr += sizeof(TRI_shape_length_list_t);

    for (p = values;  p < e;  ++p) {
      memcpy(ptr, p->_value, p->_size);
      ptr += p->_size;
    }
  }

  // homogeneous
  else if (hs) {
    TRI_homogeneous_list_shape_t* shape;

    shape = (TRI_homogeneous_list_shape_t*) TRI_Allocate(sizeof(TRI_homogeneous_list_shape_t));

    shape->base._size = sizeof(TRI_homogeneous_list_shape_t);
    shape->base._type = TRI_SHAPE_HOMOGENEOUS_LIST;
    shape->base._dataSize = TRI_SHAPE_SIZE_VARIABLE;
    shape->_sidEntry = s;

    found = shaper->findShape(shaper, &shape->base);

    if (found == 0) {
      for (p = values;  p < e;  ++p) {
        if (p->_value != 0) {
          TRI_Free(p->_value);
        }
      }

      TRI_Free(values);
      TRI_Free(shape);

      return false;
    }

    dst->_type = found->_type;
    dst->_sid = found->_sid;

    offset = sizeof(TRI_shape_length_list_t) + (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

    // copy sub-objects into data space
    * (TRI_shape_length_list_t*) ptr = n;
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
    dst->_sid = shaper->_sidList;

    offset =
      sizeof(TRI_shape_length_list_t)
      + n * sizeof(TRI_shape_sid_t)
      + (n + 1) * sizeof(TRI_shape_size_t);

    dst->_fixedSized = false;
    dst->_size = offset + total;
    dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

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

      memcpy(ptr, p->_value, p->_size);
      ptr += p->_size;
    }

    *offsets = offset;
  }

  // free TRI_shape_value_t array
  for (p = values;  p < e;  ++p) {
    if (p->_value != 0) {
      TRI_Free(p->_value);
    }
  }

  TRI_Free(values);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json array into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueArray (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::Object> json) {
  size_t n;
  size_t i;
  size_t total;

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

  // number of attributes
  v8::Handle<v8::Array> names = json->GetPropertyNames();
  n = names->Length();

  // convert into TRI_shape_value_t array
  p = (values = (TRI_shape_value_t*) TRI_Allocate(n * sizeof(TRI_shape_value_t)));
  memset(values, 0, n * sizeof(TRI_shape_value_t));

  total = 0;
  f = 0;
  v = 0;

  for (i = 0;  i < n;  ++i, ++p) {
    v8::Handle<v8::Value> key = names->Get(i);
    v8::Handle<v8::Value> val = json->Get(key);

    // first find an identifier for the name
    v8::String::Utf8Value keyStr(key);

    if (*keyStr == 0) {
      --p;
      continue;
    }

    if (TRI_EqualString(*keyStr, "_id")) {
      --p;
      continue;
    }

    p->_aid = shaper->findAttributeName(shaper, *keyStr);

    // convert value
    bool ok;

    if (p->_aid == 0) {
      ok = false;
    }
    else {
      ok = FillShapeValueJson(shaper, p, val);
    }

    if (! ok) {
      for (e = p, p = values;  p < e;  ++p) {
        if (p->_value != 0) {
          TRI_Free(p->_value);
        }
      }

      TRI_Free(values);
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

  n = f + v;

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

  a = (TRI_array_shape_t*) (ptr = (char*) TRI_Allocate(i));
  memset(ptr, 0, i);

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
  dst->_value = (ptr = (char*) TRI_Allocate(dst->_size));

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
    if (p->_value != 0) {
      TRI_Free(p->_value);
    }
  }

  TRI_Free(values);

  // lookup this shape
  found = shaper->findShape(shaper, &a->base);

  if (found == 0) {
    TRI_Free(a);
    return false;
  }

  // and finally add the sid
  dst->_sid = found->_sid;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

static bool FillShapeValueJson (TRI_shaper_t* shaper, TRI_shape_value_t* dst, v8::Handle<v8::Value> json) {
  if (json->IsNull()) {
    return FillShapeValueNull(shaper, dst);
  }

  if (json->IsBoolean()) {
    return FillShapeValueBoolean(shaper, dst, json->ToBoolean());
  }

  if (json->IsBooleanObject()) {
    v8::Handle<v8::BooleanObject> bo = v8::Handle<v8::BooleanObject>::Cast(json);
    return FillShapeValueBoolean(shaper, dst, bo);
  }

  if (json->IsNumber()) {
    return FillShapeValueNumber(shaper, dst, json->ToNumber());
  }

  if (json->IsNumberObject()) {
    v8::Handle<v8::NumberObject> no = v8::Handle<v8::NumberObject>::Cast(json);
    return FillShapeValueNumber(shaper, dst, no);
  }

  if (json->IsString()) {
    return FillShapeValueString(shaper, dst, json->ToString());
  }

  if (json->IsStringObject()) {
    v8::Handle<v8::StringObject> so = v8::Handle<v8::StringObject>::Cast(json);
    return FillShapeValueString(shaper, dst, so->StringValue());
  }

  if (json->IsArray()) {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(json);
    return FillShapeValueList(shaper, dst, array);
  }

  if (json->IsObject()) {
    return FillShapeValueArray(shaper, dst, json->ToObject());
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data null blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataNull (TRI_shaper_t* shaper,
                                                TRI_shape_t const* shape,
                                                char const* data,
                                                size_t size) {
  return v8::Null();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data boolean blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataBoolean (TRI_shaper_t* shaper,
                                                   TRI_shape_t const* shape,
                                                   char const* data,
                                                   size_t size) {
  bool v;

  v = (* (TRI_shape_boolean_t const*) data) != 0;

  return v ? v8::True() : v8::False();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data number blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataNumber (TRI_shaper_t* shaper,
                                                  TRI_shape_t const* shape,
                                                  char const* data,
                                                  size_t size) {
  TRI_shape_number_t v;

  v = * (TRI_shape_number_t const*) data;

  return v8::Number::New(v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data short string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataShortString (TRI_shaper_t* shaper,
                                                       TRI_shape_t const* shape,
                                                       char const* data,
                                                       size_t size) {
  TRI_shape_length_short_string_t l;

  l = * (TRI_shape_length_short_string_t const*) data;
  data += sizeof(TRI_shape_length_short_string_t);

  return v8::String::New(data, l - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data long string blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataLongString (TRI_shaper_t* shaper,
                                                      TRI_shape_t const* shape,
                                                      char const* data,
                                                      size_t size) {
  TRI_shape_length_long_string_t l;

  l = * (TRI_shape_length_long_string_t const*) data;
  data += sizeof(TRI_shape_length_long_string_t);

  return v8::String::New(data, l - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data array blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataArray (TRI_shaper_t* shaper,
                                                 TRI_shape_t const* shape,
                                                 char const* data,
                                                 size_t size) {
  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsetsF;
  TRI_shape_size_t const* offsetsV;
  TRI_shape_size_t f;
  TRI_shape_size_t i;
  TRI_shape_size_t n;
  TRI_shape_size_t v;
  char const* qtr;

  v8::Handle<v8::Object> array;

  s = (TRI_array_shape_t const*) shape;
  f = s->_fixedEntries;
  v = s->_variableEntries;
  n = f + v;

  qtr = (char const*) shape;
  array = v8::Object::New();

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
    v8::Handle<v8::Value> element;

    offset = *offsetsF;
    subshape = shaper->lookupShapeId(shaper, sid);
    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == 0) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == 0) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsetsF[1] - offset);

    array->Set(v8::String::New(name), element);
  }

  offsetsV = (TRI_shape_size_t const*) data;

  for (i = 0;  i < v;  ++i, ++sids, ++aids, ++offsetsV) {
    TRI_shape_sid_t sid = *sids;
    TRI_shape_aid_t aid = *aids;
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    char const* name;
    v8::Handle<v8::Value> element;

    offset = *offsetsV;
    subshape = shaper->lookupShapeId(shaper, sid);
    name = shaper->lookupAttributeId(shaper, aid);

    if (subshape == 0) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    if (name == 0) {
      LOG_WARNING("cannot find attribute #%u", (unsigned int) aid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsetsV[1] - offset);

    array->Set(v8::String::New(name), element);
  }

  return array;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataList (TRI_shaper_t* shaper,
                                                TRI_shape_t const* shape,
                                                char const* data,
                                                size_t size) {
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t const* sids;
  TRI_shape_size_t const* offsets;
  char const* ptr;
  v8::Handle<v8::Array> list;

  list = v8::Array::New();

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
    v8::Handle<v8::Value> element;

    offset = *offsets;
    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == 0) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsets[1] - offset);

    list->Set(i, element);
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataHomogeneousList (TRI_shaper_t* shaper,
                                                           TRI_shape_t const* shape,
                                                           char const* data,
                                                           size_t size) {
  TRI_homogeneous_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t const* offsets;
  char const* ptr;
  v8::Handle<v8::Array> list;

  list = v8::Array::New();

  s = (TRI_homogeneous_list_shape_t const*) shape;
  sid = s->_sidEntry;

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  ptr += sizeof(TRI_shape_length_list_t);
  offsets = (TRI_shape_size_t const*) ptr;

  for (i = 0;  i < l;  ++i, ++offsets) {
    TRI_shape_size_t offset;
    TRI_shape_t const* subshape;
    v8::Handle<v8::Value> element;

    offset = *offsets;
    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == 0) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, offsets[1] - offset);

    list->Set(i, element);
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data homogeneous sized list blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeDataHomogeneousSizedList (TRI_shaper_t* shaper,
                                                                TRI_shape_t const* shape,
                                                                char const* data,
                                                                size_t size) {
  TRI_homogeneous_sized_list_shape_t const* s;
  TRI_shape_length_list_t i;
  TRI_shape_length_list_t l;
  TRI_shape_sid_t sid;
  TRI_shape_size_t length;
  TRI_shape_size_t offset;
  char const* ptr;
  v8::Handle<v8::Array> list;

  list = v8::Array::New();

  s = (TRI_homogeneous_sized_list_shape_t const*) shape;
  sid = s->_sidEntry;

  ptr = data;
  l = * (TRI_shape_length_list_t const*) ptr;

  length = s->_sizeEntry;
  offset = sizeof(TRI_shape_length_list_t);

  for (i = 0;  i < l;  ++i, offset += length) {
    TRI_shape_t const* subshape;
    v8::Handle<v8::Value> element;

    subshape = shaper->lookupShapeId(shaper, sid);

    if (subshape == 0) {
      LOG_WARNING("cannot find shape #%u", (unsigned int) sid);
      continue;
    }

    element = JsonShapeData(shaper, subshape, data + offset, length);

    list->Set(i, element);
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a data blob into a json object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JsonShapeData (TRI_shaper_t* shaper,
                                            TRI_shape_t const* shape,
                                            char const* data,
                                            size_t size) {
  if (shape == 0) {
    return v8::Null();
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

  return v8::Null();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell V8 Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NULL into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectJsonNull (TRI_json_t const* json) {
  return v8::Null();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t BOOLEAN into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectJsonBoolean (TRI_json_t const* json) {
  return json->_value._boolean ? v8::True() : v8::False();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NUMBER into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectJsonNumber (TRI_json_t const* json) {
  return v8::Number::New(json->_value._number);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NUMBER into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectJsonString (TRI_json_t const* json) {
  return v8::String::New(json->_value._string.data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t ARRAY into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectJsonArray (TRI_json_t const* json) {
  v8::Handle<v8::Object> object = v8::Object::New();

  size_t n = TRI_SizeVector(&json->_value._objects);

  for (size_t i = 0;  i < n;  i += 2) {
    TRI_json_t* key = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    TRI_json_t* j = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
    v8::Handle<v8::Value> val = ObjectJson(j);

    object->Set(v8::String::New(key->_value._string.data), val);
  }

    return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t LIST into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectJsonList (TRI_json_t const* json) {
  v8::Handle<v8::Array> object = v8::Array::New();

  size_t n = TRI_SizeVector(&json->_value._objects);

  for (size_t i = 0;  i < n;  ++i) {
    TRI_json_t* j = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);
    v8::Handle<v8::Value> val = ObjectJson(j);

    object->Set(i, val);
  }

    return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectJson (TRI_json_t const* json) {
  v8::HandleScope scope;

  switch (json->_type) {
    case TRI_JSON_NULL:
      return scope.Close(ObjectJsonNull(json));

    case TRI_JSON_BOOLEAN:
      return scope.Close(ObjectJsonBoolean(json));

    case TRI_JSON_NUMBER:
      return scope.Close(ObjectJsonNumber(json));

    case TRI_JSON_STRING:
      return scope.Close(ObjectJsonString(json));

    case TRI_JSON_ARRAY:
      return scope.Close(ObjectJsonArray(json));

    case TRI_JSON_LIST:
      return scope.Close(ObjectJsonList(json));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_shaped_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ObjectShapedJson (TRI_doc_collection_t* collection,
                                        TRI_voc_did_t did,
                                        TRI_shaper_t* shaper,
                                        TRI_shaped_json_t const* shaped) {
  TRI_shape_t const* shape;

  shape = shaper->lookupShapeId(shaper, shaped->_sid);

  if (shape == 0) {
    LOG_WARNING("cannot find shape #%u", (unsigned int) shaped->_sid);
    return v8::Null();
  }

  v8::Handle<v8::Value> result = JsonShapeData(shaper, shape, shaped->_data.data, shaped->_data.length);

  if (result->IsObject()) {
    char* cidStr = TRI_StringUInt64(collection->base._cid);
    char* didStr = TRI_StringUInt64(did);

    string name = cidStr + string(":") + didStr;

    TRI_FreeString(didStr);
    TRI_FreeString(cidStr);

    result->ToObject()->Set(DidKey, v8::String::New(name.c_str()));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_result_set_t into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Array> ArrayResultSet (TRI_result_set_t* rs) {
  v8::Handle<v8::Array> array = v8::Array::New();

  TRI_doc_collection_t* collection = rs->_container->_collection;
  TRI_shaper_t* shaper = collection->_shaper;

  size_t pos;

  for (pos = 0;  rs->hasNext(rs);  ++pos) {
    TRI_voc_did_t did;
    TRI_json_t const* augmented;
    TRI_shaped_json_t* element = rs->next(rs, &did, &augmented);

    v8::Handle<v8::Value> object = ObjectShapedJson(collection, did, shaper, element);

    if (augmented != NULL) {
      AugmentObject(object, augmented);
    }

    array->Set(pos, object);
  }

  return array;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* ShapedJsonV8Object (v8::Handle<v8::Value> object, TRI_shaper_t* shaper) {
  TRI_shape_value_t dst;
  bool ok = FillShapeValueJson(shaper, &dst, object);

  if (! ok) {
    return 0;
  }


  TRI_shaped_json_t* shaped = (TRI_shaped_json_t*) TRI_Allocate(sizeof(TRI_shaped_json_t));

  shaped->_sid = dst._sid;
  shaped->_data.length = dst._size;
  shaped->_data.data = dst._value;

  return shaped;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a string
////////////////////////////////////////////////////////////////////////////////

string ObjectToString (v8::Handle<v8::Value> value) {
  v8::String::Utf8Value utf8Value(value);

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

char ObjectToCharacter (v8::Handle<v8::Value> value, bool& error) {
  error = false;

  if (! value->IsString() && ! value->IsStringObject()) {
    error = true;
    return '\0';
  }

  v8::String::Utf8Value sep(value->ToString());

  if (*sep == 0 || sep.length() != 1) {
    error = true;
    return '\0';
  }

  return (*sep)[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a double
////////////////////////////////////////////////////////////////////////////////

double ObjectToDouble (v8::Handle<v8::Value> value) {
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
/// @brief converts an V8 object to a double
////////////////////////////////////////////////////////////////////////////////

double ObjectToDouble (v8::Handle<v8::Value> value, bool& error) {
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

bool ObjectToBoolean (v8::Handle<v8::Value> value) {
  if (value->IsBoolean()) {
    return value->ToBoolean()->Value();
  }
  else if (value->IsBooleanObject()) {
    v8::Handle<v8::BooleanObject> bo = v8::Handle<v8::BooleanObject>::Cast(value);
    return bo->BooleanValue();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   IMPORT / EXPORT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell V8 Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief begins a new CSV line
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvBegin (TRI_csv_parser_t* parser, size_t row) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array) = v8::Array::New();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new CSV field
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvAdd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(column, v8::String::New(field));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ends a CSV line
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvEnd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(column, v8::String::New(field));

  v8::Handle<v8::Function>* cb = reinterpret_cast<v8::Handle<v8::Function>*>(parser->_dataEnd);
  v8::Handle<v8::Number> r = v8::Number::New(row);

  v8::Handle<v8::Value> args[] = { *array, r };
  (*cb)->Call(*cb, 2, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell V8 Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a CSV file
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessCsvFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: processCsvFile(<filename>, <callback>[, <options>])")));
  }

  // extract the filename
  v8::String::Utf8Value filename(argv[0]);

  if (*filename == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be an UTF8 filename")));
  }

  // extract the callback
  v8::Handle<v8::Function> cb = v8::Handle<v8::Function>::Cast(argv[1]);

  // extract the options
  v8::Handle<v8::String> separatorKey = v8::String::New("separator");
  v8::Handle<v8::String> quoteKey = v8::String::New("quote");

  char separator = ',';
  char quote = '"';

  if (3 <= argv.Length()) {
    v8::Handle<v8::Object> options = argv[2]->ToObject();
    bool error;

    // separator
    if (options->Has(separatorKey)) {
      separator = ObjectToCharacter(options->Get(separatorKey), error);

      if (error) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.separator must be a character")));
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = ObjectToCharacter(options->Get(quoteKey), error);

      if (error) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.quote must be a character")));
      }
    }
  }

  // read and convert
  int fd = open(*filename, O_RDONLY);

  if (fd < 0) {
    return scope.Close(v8::ThrowException(v8::String::New(TRI_LAST_ERROR_STR)));
  }

  TRI_csv_parser_t parser;

  TRI_InitCsvParser(&parser,
                    ProcessCsvBegin,
                    ProcessCsvAdd,
                    ProcessCsvEnd);

  parser._separator = separator;
  parser._quote = quote;

  parser._dataEnd = &cb;

  v8::Handle<v8::Array> array;
  parser._dataBegin = &array;

  char buffer[10240];

  while (true) {
    v8::HandleScope scope;

    ssize_t n = read(fd, buffer, sizeof(buffer));

    if (n < 0) {
      TRI_DestroyCsvParser(&parser);
      return scope.Close(v8::ThrowException(v8::String::New(TRI_LAST_ERROR_STR)));
    }
    else if (n == 0) {
      TRI_DestroyCsvParser(&parser);
      break;
    }

    TRI_ParseCsvString2(&parser, buffer, n);
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a JSON file
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessJsonFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: processJsonFile(<filename>, <callback>)")));
  }

  // extract the filename
  v8::String::Utf8Value filename(argv[0]);

  if (*filename == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be an UTF8 filename")));
  }

  // extract the callback
  v8::Handle<v8::Function> cb = v8::Handle<v8::Function>::Cast(argv[1]);

  // read and convert
  string line;
  ifstream file(*filename);

  if (file.is_open()) {
    size_t row = 0;

    while (file.good()) {
      v8::HandleScope scope;

      getline(file, line);

      char const* ptr = line.c_str();
      char const* end = ptr + line.length();

      while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r')) {
        ++ptr;
      }

      if (ptr == end) {
        continue;
      }

      char* error;
      v8::Handle<v8::Value> object = TRI_FromJsonString(line.c_str(), &error);

      if (object->IsUndefined()) {
        v8::Handle<v8::String> msg = v8::String::New(error);
        TRI_FreeString(error);
        return scope.Close(v8::ThrowException(msg));
      }

      v8::Handle<v8::Number> r = v8::Number::New(row);
      v8::Handle<v8::Value> args[] = { object, r };
      cb->Call(cb, 2, args);

      row++;
    }

    file.close();
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("cannot open file")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell V8 Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the log-level
///
/// <b><tt>logLevel()</tt></b>
///
/// Returns the current log-level as string.
///
/// @verbinclude fluent37
///
/// <b><tt>logLevel(level)</tt></b>
///
/// Changes the current log-level. Valid log-level are:
///
/// - fatal
/// - error
/// - warning
/// - info
/// - debug
/// - trace
///
/// @verbinclude fluent38
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LogLevel (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (1 <= argv.Length()) {
    v8::String::Utf8Value str(argv[0]);

    TRI_SetLogLevelLogging(*str);
  }

  return scope.Close(v8::String::New(TRI_GetLogLevelLogging()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts JSON string to object
///
/// <b><tt>fromJson(string)</tt></b>
///
/// Converts a string representation of a JSON object into a JavaScript
/// object.
///
/// @verbinclude fluent35
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FromJson (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: fromJson(<string>)")));
  }

  v8::String::Utf8Value str(argv[0]);

  if (*str == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot get UTF-8 representation of <string>")));
  }

  char* error;
  v8::Handle<v8::Value> object = TRI_FromJsonString(*str, &error);

  if (object->IsUndefined()) {
    v8::Handle<v8::String> msg = v8::String::New(error);
    TRI_FreeString(error);
    return scope.Close(v8::ThrowException(msg));
  }

  return scope.Close(object);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
///
/// <b><tt>output(string1, string2, string3, ...)</tt></b>
///
/// Outputs the arguments to standard output.
///
/// @verbinclude fluent39
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Output (v8::Arguments const& argv) {
  for (int i = 0; i < argv.Length(); i++) {
    v8::HandleScope scope;

    // extract the next argument
    v8::Handle<v8::Value> val = argv[i];

    // convert it into a string
    v8::String::Utf8Value utf8(val);

    if (*utf8 == 0) {
      continue;
    }

    // print the argument
    char const* ptr = *utf8;
    size_t len = utf8.length();

    while (0 < len) {
      ssize_t n = write(1, ptr, len);

      if (n < 0) {
        return v8::Undefined();
      }

      len -= n;
      ptr += n;
    }
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the arguments
///
/// <b><tt>print(arg1, arg2, arg3, ...)</tt></b>
///
/// Prints the arguments. If an argument is an object having a function print,
/// then this function is called. Otherwise @c toJson is used.  After each
/// argument a newline is printed.
///
/// @verbinclude fluent40
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Print (v8::Arguments const& argv) {
  v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(
    argv.Holder()->CreationContext()->Global()->Get(v8::String::New("toJson")));

  for (int i = 0; i < argv.Length(); i++) {
    v8::HandleScope scope;
    TRI_string_buffer_t buffer;
    bool printBuffer = true;

    TRI_InitStringBuffer(&buffer);

    // extract the next argument
    v8::Handle<v8::Value> val = argv[i];

    // convert the object into json - if possible
    if (val->IsObject()) {
      v8::Handle<v8::Object> obj = val->ToObject();
      v8::Handle<v8::String> printFuncName = v8::String::New("print");

      if (obj->Has(printFuncName)) {
        v8::Handle<v8::Function> print = v8::Handle<v8::Function>::Cast(obj->Get(printFuncName));
        v8::Handle<v8::Value> args[] = { val };
        print->Call(obj, 1, args);
        printBuffer = false;
      }
      else {
        v8::Handle<v8::Value> args[] = { val };
        v8::Handle<v8::Value> cnv = toJson->Call(toJson, 1, args);

        v8::String::Utf8Value utf8(cnv);

        if (*utf8 == 0) {
          TRI_AppendStringStringBuffer(&buffer, "[object]");
        }
        else {
          TRI_AppendString2StringBuffer(&buffer, *utf8, utf8.length());
        }
      }
    }
    else if (val->IsString()) {
      v8::String::Utf8Value utf8(val->ToString());

      if (*utf8 == 0) {
        TRI_AppendStringStringBuffer(&buffer, "[string]");
      }
      else {
        TRI_AppendCharStringBuffer(&buffer, '"');
        TRI_AppendString2StringBuffer(&buffer, *utf8, utf8.length());
        TRI_AppendCharStringBuffer(&buffer, '"');
      }
    }
    else {
      v8::String::Utf8Value utf8(val);

      if (*utf8 == 0) {
        TRI_AppendStringStringBuffer(&buffer, "[value]");
      }
      else {
        TRI_AppendString2StringBuffer(&buffer, *utf8, utf8.length());
      }
    }

    // print the argument
    if (printBuffer) {
      char const* ptr = TRI_BeginStringBuffer(&buffer);
      size_t len = TRI_LengthStringBuffer(&buffer);

      while (0 < len) {
        ssize_t n = write(1, ptr, len);

        if (n < 0) {
          TRI_DestroyStringBuffer(&buffer);
          return v8::Undefined();
        }

        len -= n;
        ptr += n;
      }
    }

    // print a new line
    ssize_t n = write(1, "\n", 1);

    if (n < 0) {
      TRI_DestroyStringBuffer(&buffer);
      return v8::Undefined();
    }

    TRI_DestroyStringBuffer(&buffer);
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current time
///
/// <b><tt>time()</tt></b>
///
/// Returns the current time in seconds.
///
/// @verbinclude fluent36
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Time (v8::Arguments const& argv) {
  v8::HandleScope scope;

  return scope.Close(v8::Number::New(TRI_microtime()));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell V8 Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds attributes to array
////////////////////////////////////////////////////////////////////////////////

void AugmentObject (v8::Handle<v8::Value> value, TRI_json_t const* json) {
  v8::HandleScope scope;

  if (! value->IsObject()) {
    return;
  }

  if (json->_type != TRI_JSON_ARRAY) {
    return;
  }

  v8::Handle<v8::Object> object = value->ToObject();

  size_t n = TRI_SizeVector(&json->_value._objects);

  for (size_t i = 0;  i < n;  i += 2) {
    TRI_json_t* key = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      continue;
    }

    TRI_json_t* j = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
    v8::Handle<v8::Value> val = ObjectJson(j);

    object->Set(v8::String::New(key->_value._string.data), val);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 shell functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void InitV8Shell (v8::Handle<v8::ObjectTemplate> global) {

  // .............................................................................
  // create the global functions
  // .............................................................................

  global->Set(v8::String::New("logLevel"),
              v8::FunctionTemplate::New(JS_LogLevel),
              v8::ReadOnly);

  global->Set(v8::String::New("output"),
              v8::FunctionTemplate::New(JS_Output),
              v8::ReadOnly);

  global->Set(v8::String::New("print"),
              v8::FunctionTemplate::New(JS_Print),
              v8::ReadOnly);

  global->Set(v8::String::New("processCsvFile"),
              v8::FunctionTemplate::New(JS_ProcessCsvFile),
              v8::ReadOnly);

  global->Set(v8::String::New("processJsonFile"),
              v8::FunctionTemplate::New(JS_ProcessJsonFile),
              v8::ReadOnly);

  global->Set(v8::String::New("fromJson"),
              v8::FunctionTemplate::New(JS_FromJson),
              v8::ReadOnly);

  global->Set(v8::String::New("time"),
              v8::FunctionTemplate::New(JS_Time),
              v8::ReadOnly);

  // .............................................................................
  // keys
  // .............................................................................

  DidKey = v8::Persistent<v8::String>::New(v8::String::New("_id"));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
