////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
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
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbase.h"

#define TRI_WITHIN_C
#include <Basics/conversions.h>
#include <Basics/logging.h>
#include <Basics/strings.h>
#include <Basics/csv.h>

#include <VocBase/query.h>
#include <VocBase/simple-collection.h>
#include <VocBase/vocbase.h>

#include "ShapedJson/shaped-json.h"

#include "V8/v8-shell.h"
#undef TRI_WITHIN_C

#include <regex.h>

#include <map>
#include <string>

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
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "type"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_TYPE = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "operand"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_OPERAND = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "query"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_RESULT_SET = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "document" (shared)
////////////////////////////////////////////////////////////////////////////////

static int SLOT_DOCUMENT = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "limit" (shared)
////////////////////////////////////////////////////////////////////////////////

static int SLOT_LIMIT = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "skip" (shared)
////////////////////////////////////////////////////////////////////////////////

static int SLOT_SKIP = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "latitiude"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_LATITUDE = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "longitude"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_LONGITUDE = 6;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "locLatitude" (shared)
////////////////////////////////////////////////////////////////////////////////

static int SLOT_LOCATION = 7;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "locLatitude" (shared)
////////////////////////////////////////////////////////////////////////////////

static int SLOT_LOC_LATITUDE = 7;

////////////////////////////////////////////////////////////////////////////////
/// @brief template slot "locLongitude"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_LOC_LONGITUDE = 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief end marker
////////////////////////////////////////////////////////////////////////////////

static int SLOT_END = 8;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result set mapping for weak pointers
////////////////////////////////////////////////////////////////////////////////

static map< TRI_result_set_t*, v8::Persistent<v8::Object> > JSResultSets;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  OBJECT TEMPLATES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief query class template
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> QueryTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief result-set class template
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> ResultSetTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief TRI_vocbase_col_t template
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> VocbaseColTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief TRI_vocbase_t class template
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> VocBaseTempl;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    FUNCTION NAMES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief "count" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> CountFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "document" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> DocumentFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "ensureGeoIndex" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> EnsureGeoIndexFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "explain" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> ExplainFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "hasNext" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> HasNextFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "limit" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> LimitFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "near" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> NearFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "next" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> NextFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "output" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> OutputFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "parameter" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> ParameterFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "print" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> PrintFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "printQuery" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> PrintQueryFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "save" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SaveFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "select" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SelectFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "show" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> ShowFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "skip" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SkipFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "toArray" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> ToArrayFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "toString" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> ToStringFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY TYPE NAMES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief "collection" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> CollectionQueryType;

////////////////////////////////////////////////////////////////////////////////
/// @brief "document" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> DocumentQueryType;

////////////////////////////////////////////////////////////////////////////////
/// @brief "select" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SelectQueryType;

////////////////////////////////////////////////////////////////////////////////
/// @brief "limit" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> LimitQueryType;

////////////////////////////////////////////////////////////////////////////////
/// @brief "near" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> NearQueryType;

////////////////////////////////////////////////////////////////////////////////
/// @brief "near2" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> Near2QueryType;

////////////////////////////////////////////////////////////////////////////////
/// @brief "skip" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SkipQueryType;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         KEY NAMES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief "did" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> DidKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "journalSize" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> JournalSizeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "syncAfterBytes" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SyncAfterBytesKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "syncAfterObjects" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SyncAfterObjectsKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "syncAfterTime" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SyncAfterTimeKey;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               REGULAR EXPRESSIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief document identifier as collection-id:document-id
////////////////////////////////////////////////////////////////////////////////

static regex_t DocumentIdRegex;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a C++ into a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static v8::Handle<v8::Object> WrapClass (v8::Persistent<v8::ObjectTemplate> classTempl,
                                         T* y) {

  // handle scope for temporary handles
  v8::HandleScope scope;

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = classTempl->NewInstance();
  v8::Handle<v8::External> classPtr = v8::External::New(static_cast<T*>(y));

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(0, classPtr);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps a C++ class given a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static T* UnwrapClass (v8::Handle<v8::Object> obj) {
  v8::Handle<v8::External> field;
  field = v8::Handle<v8::External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();

  return static_cast<T*>(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calls the "toString" function
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> PrintUsingToString (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = argv.Holder();
  v8::Handle<v8::Function> toString = v8::Handle<v8::Function>::Cast(self->Get(ToStringFuncName));

  v8::Handle<v8::Value> args1[] = { self };
  v8::Handle<v8::Value> str = toString->Call(self, 1, args1);

  v8::Handle<v8::Function> output = v8::Handle<v8::Function>::Cast(self->CreationContext()->Global()->Get(OutputFuncName));

  v8::Handle<v8::Value> args2[] = { str, v8::String::New("\n") };
  output->Call(output, 2, args2);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsDocumentId (v8::Handle<v8::Value> arg, TRI_voc_cid_t& cid, TRI_voc_did_t& did) {
  if (arg->IsNumber()) {
    cid = 0;
    did = arg->ToNumber()->Value();
    return true;
  }

  if (! arg->IsString()) {
    return false;
  }

  v8::String::Utf8Value str(arg);
  char const* s = *str;

  if (s == 0) {
    return false;
  }

  regmatch_t matches[3];

  if (regexec(&DocumentIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    cid = TRI_UInt64String2(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    did = TRI_UInt64String2(s + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t const* LoadCollection (v8::Handle<v8::Object> collection,
                                                v8::Handle<v8::String>* err) {
  TRI_vocbase_col_t const* col;
  bool ok;

  col = UnwrapClass<TRI_vocbase_col_t>(collection);

  if (col == 0) {
    *err = v8::String::New("illegal collection pointer");
    return 0;
  }

  if (col->_corrupted) {
    *err = v8::String::New("collection is corrupted, please run collection check");
    return 0;
  }

  if (col->_loaded) {
    if (col->_collection == 0) {
      *err = v8::String::New("cannot load collection, check log");
      return 0;
    }

    return col;
  }

  if (col->_newBorn) {
    LOG_INFO("creating new collection: '%s'", col->_name);

    ok = TRI_ManifestCollectionVocBase(col->_vocbase, col);

    if (! ok) {
      string msg = "cannot create collection: " + string(TRI_last_error());
      *err = v8::String::New(msg.c_str());
      return 0;
    }

    LOG_INFO("collection created");
  }
  else {
    LOG_INFO("loading collection: '%s'", col->_name);

    col = TRI_LoadCollectionVocBase(col->_vocbase, col->_name);

    LOG_INFO("collection loaded");
  }

  if (col == 0 || col->_collection == 0) {
    *err = v8::String::New("cannot load collection, check log");
    return 0;
  }

  if (col->_corrupted) {
    *err = v8::String::New("collection is corrupted, please run collection check");
    return 0;
  }

  return col;
}

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
/// @addtogroup VocBasePrivate VocBase (Private)
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
/// @brief converts a shaped json into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectShapedJson (TRI_doc_collection_t* collection,
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
/// @brief converts a result-set into a V8 array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> ArrayResultSet (TRI_result_set_t* rs) {
  v8::Handle<v8::Array> array = v8::Array::New();

  TRI_doc_collection_t* collection = rs->_container->_collection;
  TRI_shaper_t* shaper = collection->_shaper;

  size_t pos;

  for (pos = 0;  rs->hasNext(rs);  ++pos) {
    TRI_voc_did_t did;
    TRI_shaped_json_t* element = rs->next(rs, &did);
    v8::Handle<v8::Value> object = ObjectShapedJson(collection, did, shaper, element);

    array->Set(pos, object);
  }

  return array;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   QUERY FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify query
////////////////////////////////////////////////////////////////////////////////

static string StringifyQuery (v8::Handle<v8::Value> query) {
  if (! query->IsObject()) {
    return "[unknown]";
  }

  v8::Handle<v8::Object> self = query->ToObject();

  if (self->InternalFieldCount() <= SLOT_TYPE) {
    return "[unknown]";
  }

  v8::Handle<v8::Value> type = self->GetInternalField(SLOT_TYPE);
  string result;

  if (type == CollectionQueryType) {
    v8::Handle<v8::String> err;
    TRI_vocbase_col_t const* collection = LoadCollection(self, &err);

    if (collection == 0) {
      return "[unknown]";
    }

    result += "[collection \"" + string(collection->_name) + "\"]";
  }
  else {
    if (self->InternalFieldCount() <= SLOT_OPERAND) {
      return "[unknown]";
    }

    v8::Handle<v8::Value> operand = self->GetInternalField(SLOT_OPERAND);

    result += StringifyQuery(operand);

    // document query
    if (type == DocumentQueryType) {
      if (self->InternalFieldCount() <= SLOT_DOCUMENT) {
        return "[unknown]";
      }

      v8::Handle<v8::Value> val = self->GetInternalField(SLOT_DOCUMENT);
      result += ".document(" + ObjectToString(val) + ")";
    }

    // limit query
    else if (type == LimitQueryType) {
      if (self->InternalFieldCount() <= SLOT_LIMIT) {
        return "[unknown]";
      }

      v8::Handle<v8::Value> val = self->GetInternalField(SLOT_LIMIT);
      result += ".limit(" + ObjectToString(val) + ")";
    }

    // near query (list)
    else if (type == NearQueryType) {
      if (self->InternalFieldCount() <= SLOT_LOCATION) {
        return "[unknown]";
      }

      v8::Handle<v8::Value> lat = self->GetInternalField(SLOT_LATITUDE);
      v8::Handle<v8::Value> lon = self->GetInternalField(SLOT_LONGITUDE);
      v8::Handle<v8::Value> loc = self->GetInternalField(SLOT_LOCATION);
      result += ".near(\"" + ObjectToString(loc) + "\", " + ObjectToString(lat) + ", " + ObjectToString(lon) + ")";
    }

    // near query (array)
    else if (type == Near2QueryType) {
      if (self->InternalFieldCount() <= SLOT_LOC_LONGITUDE) {
        return "[unknown]";
      }

      v8::Handle<v8::Value> lat = self->GetInternalField(SLOT_LATITUDE);
      v8::Handle<v8::Value> lon = self->GetInternalField(SLOT_LONGITUDE);
      v8::Handle<v8::Value> locLat = self->GetInternalField(SLOT_LOC_LATITUDE);
      v8::Handle<v8::Value> locLon = self->GetInternalField(SLOT_LOC_LONGITUDE);
      result += ".near(\""
                + ObjectToString(locLat) + "\", " + ObjectToString(lat) + ", \""
                + ObjectToString(locLon) + "\", " + ObjectToString(lon) + ")";
    }

    // select query
    else if (type == SelectQueryType) {
      result += ".select()";
    }

    // skip query
    else if (type == SkipQueryType) {
      if (self->InternalFieldCount() <= SLOT_SKIP) {
        return "[unknown]";
      }

      v8::Handle<v8::Value> val = self->GetInternalField(SLOT_SKIP);
      result += ".skip(" + ObjectToString(val) + ")";
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CreateQuery (v8::Handle<v8::Value> query, v8::Handle<v8::String>* err) {
  if (! query->IsObject()) {
    *err = v8::String::New("corrupted query");
    return 0;
  }

  v8::Handle<v8::Object> self = query->ToObject();

  if (self->InternalFieldCount() <= SLOT_TYPE) {
    *err = v8::String::New("corrupted query");
    return 0;
  }

  v8::Handle<v8::Value> type = self->GetInternalField(SLOT_TYPE);

  // the "query" is the whole collection
  if (type == CollectionQueryType) {
    TRI_vocbase_col_t const* collection = LoadCollection(self, err);

    if (collection == 0) {
      return 0;
    }

    return TRI_CreateCollectionQuery(collection->_collection);
  }
  else {
    if (self->InternalFieldCount() <= SLOT_OPERAND) {
      *err = v8::String::New("corrupted query, missing operand");
      return 0;
    }

    v8::Handle<v8::Value> operand = self->GetInternalField(SLOT_OPERAND);

    TRI_query_t* opQuery = CreateQuery(operand, err);

    if (opQuery == 0) {
      return 0;
    }

    // select document by idenifier
    if (type == DocumentQueryType) {
      if (self->InternalFieldCount() <= SLOT_DOCUMENT) {
        *err = v8::String::New("corrupted query, missing document identifier");
        return 0;
      }

      TRI_voc_cid_t cid = 0;
      TRI_voc_did_t did = 0;
      v8::Handle<v8::Value> idVal = self->GetInternalField(SLOT_DOCUMENT);

      if (IsDocumentId(idVal, cid, did)) {
        if (cid != 0 && cid != opQuery->_collection->base._cid) {
          *err = v8::String::New("cannot execute cross collection query");
          return 0;
        }
      }

      return TRI_CreateDocumentQuery(opQuery, did);
    }

    // limit the result set
    else if (type == LimitQueryType) {
      if (self->InternalFieldCount() <= SLOT_LIMIT) {
        *err = v8::String::New("corrupted query, missing limit");
        return 0;
      }

      double limit = ObjectToDouble(self->GetInternalField(SLOT_LIMIT));

      return TRI_CreateLimitQuery(opQuery, (TRI_voc_ssize_t) limit);
    }

    // near a given point (list)
    else if (type == NearQueryType) {
      if (self->InternalFieldCount() <= SLOT_LOCATION) {
        *err = v8::String::New("corrupted query, missing location");
        return 0;
      }

      double count = ObjectToDouble(self->GetInternalField(SLOT_LIMIT));
      double latitiude = ObjectToDouble(self->GetInternalField(SLOT_LATITUDE));
      double longitude = ObjectToDouble(self->GetInternalField(SLOT_LONGITUDE));
      string location = ObjectToString(self->GetInternalField(SLOT_LOCATION));

      char const* errorMsg;
      TRI_query_t* query = TRI_CreateNearQuery(opQuery, location.c_str(), latitiude, longitude, count, &errorMsg);

      if (query == 0) {
        *err = v8::String::New(errorMsg);
        return 0;
      }
      else {
        return query;
      }
    }

    // near a given point (array)
    else if (type == Near2QueryType) {
      if (self->InternalFieldCount() <= SLOT_LOC_LONGITUDE) {
        *err = v8::String::New("corrupted query, missing location");
        return 0;
      }

      double count = ObjectToDouble(self->GetInternalField(SLOT_LIMIT));

      double latitiude = ObjectToDouble(self->GetInternalField(SLOT_LATITUDE));
      string locLatitude = ObjectToString(self->GetInternalField(SLOT_LOC_LATITUDE));

      double longitude = ObjectToDouble(self->GetInternalField(SLOT_LONGITUDE));
      string locLongitude = ObjectToString(self->GetInternalField(SLOT_LOC_LONGITUDE));

      char const* errorMsg;
      TRI_query_t* query = TRI_CreateNear2Query(opQuery, locLatitude.c_str(), latitiude, locLongitude.c_str(), longitude, count, &errorMsg);

      if (query == 0) {
        *err = v8::String::New(errorMsg);
        return 0;
      }
      else {
        return query;
      }
    }


    // selection by example
    else if (type == SelectQueryType) {
      return opQuery;
    }

    // skip the result set
    else if (type == SkipQueryType) {
      if (self->InternalFieldCount() <= SLOT_SKIP) {
        *err = v8::String::New("corrupted query, missing skip value");
        return 0;
      }

      double skip = ObjectToDouble(self->GetInternalField(SLOT_SKIP));

      if (skip <= 0.0) {
        return TRI_CreateSkipQuery(opQuery, 0);
      }
      else {
        return TRI_CreateSkipQuery(opQuery, (TRI_voc_size_t) skip);
      }
    }

    *err = v8::String::New("corrupted query, unknown query type");
    return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback
////////////////////////////////////////////////////////////////////////////////

static void WeakResultSetCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_result_set_t* rs = (TRI_result_set_t*) parameter;

  LOG_TRACE("weak result set callback called");

  // find the persistent handle
  v8::Persistent<v8::Object> rsp = JSResultSets[rs];
  JSResultSets.erase(rs);

  // clear the internal field holding the result set
  rsp->SetInternalField(0, v8::Null());

  // dispose and clear the persistent handle
  rsp.Dispose();
  rsp.Clear();

  // and free the result set
  rs->free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query or uses existing result set
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CheckAndExecuteQuery (v8::Handle<v8::Value> query, v8::Handle<v8::String>* err) {
  v8::Handle<v8::Object> self = query->ToObject();

  if (self->InternalFieldCount() <= SLOT_RESULT_SET) {
    *err = v8::String::New("corrupted query");
    return v8::Null();
  }

  v8::Handle<v8::Value> value = self->GetInternalField(SLOT_RESULT_SET);

  if (value->IsNull()) {
    LOG_TRACE("creating query");

    // create a TRI_query from the query description
    TRI_query_t* q = CreateQuery(query, err);

    if (q == 0) {
      return v8::Undefined();
    }

    // execute the query and get a result set
    TRI_result_set_t* rs = TRI_ExecuteQuery(q);

    if (rs == 0) {
      *err = v8::String::New("cannot execute query");
      return v8::Undefined();
    }

    // check if we know this set, otherwise construct a result set object
    if (JSResultSets.find(rs) == JSResultSets.end()) {
      v8::Persistent<v8::Object> rsp = v8::Persistent<v8::Object>::New(WrapClass(ResultSetTempl, rs));

      self->SetInternalField(SLOT_RESULT_SET, rsp);
      rsp.MakeWeak(rs, WeakResultSetCallback);
      JSResultSets[rs] = rsp;

      return rsp;
    }
    else {
      v8::Persistent<v8::Object> rsp = JSResultSets[rs];
      self->SetInternalField(SLOT_RESULT_SET, rsp);
      return rsp;
    }
  }
  else {
    return value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if a query has been executed
////////////////////////////////////////////////////////////////////////////////

static bool QueryExecuted (v8::Handle<v8::Object> query) {
  if (query->InternalFieldCount() <= SLOT_RESULT_SET) {
    return false;
  }

  v8::Handle<v8::Value> value = query->GetInternalField(SLOT_RESULT_SET);

  return value->IsNull() ? false : true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief counts an existing query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CountQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> query = argv.Holder();
  v8::Handle<v8::String> err;

  v8::Handle<v8::Value> q = CheckAndExecuteQuery(argv.Holder(), &err);

  if (q->IsUndefined()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_result_set_t* rs = UnwrapClass<TRI_result_set_t>(q->ToObject());

  if (rs == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute query")));
  }

  if (0 < argv.Length() && argv[0]->IsTrue()) {
    return scope.Close(v8::Number::New(rs->count(rs, true)));
  }
  else {
    return scope.Close(v8::Number::New(rs->count(rs, false)));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DocumentQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> query = argv.Holder();

  // query should be un-executed
  if (QueryExecuted(query)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // first and only argument schould be an integer
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: document(<id>)")));
  }

  v8::Handle<v8::Value> arg1 = argv[0];
  TRI_voc_cid_t cid;
  TRI_voc_did_t did;

  if (! IsDocumentId(arg1, cid, did)) {
    return scope.Close(v8::ThrowException(v8::String::New("<id> must be a document identifier")));
  }

  v8::Handle<v8::Object> result = QueryTempl->NewInstance();

  result->SetInternalField(SLOT_TYPE, DocumentQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());
  result->SetInternalField(SLOT_OPERAND, query);
  result->SetInternalField(SLOT_DOCUMENT, arg1);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explains an existing query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExplainQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> query = argv.Holder();
  v8::Handle<v8::String> err;

  v8::Handle<v8::Value> q = CheckAndExecuteQuery(argv.Holder(), &err);

  if (q->IsUndefined()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_result_set_t* rs = UnwrapClass<TRI_result_set_t>(q->ToObject());

  if (rs == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute query")));
  }

  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(v8::String::New("cursor"), v8::String::New(rs->_info._cursor));
  result->Set(v8::String::New("scannedIndexEntries"), v8::Number::New(rs->_info._scannedIndexEntries));
  result->Set(v8::String::New("scannedDocuments"), v8::Number::New(rs->_info._scannedDocuments));
  result->Set(v8::String::New("matchedDocuments"), v8::Number::New(rs->_info._matchedDocuments));
  result->Set(v8::String::New("runtime"), v8::Number::New(rs->_info._runtime));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query and checks for next result
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> HasNextQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::Handle<v8::Object> self = argv.Holder();
  v8::Handle<v8::String> err;

  v8::Handle<v8::Value> q = CheckAndExecuteQuery(argv.Holder(), &err);

  if (q->IsUndefined()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_result_set_t* rs = UnwrapClass<TRI_result_set_t>(q->ToObject());

  if (rs == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute query")));
  }
  else {
    return scope.Close(rs->hasNext(rs) ? v8::True() : v8::False());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> LimitQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> query = argv.Holder();

  // query should be un-executed
  if (QueryExecuted(query)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // first and only argument schould be an integer
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: limit(<limit>)")));
  }

  v8::Handle<v8::Value> arg1 = argv[0];

  if (! arg1->IsNumber()) {
    return scope.Close(v8::ThrowException(v8::Exception::TypeError(v8::String::New("<limit> must be a number"))));
  }

  double limit = arg1->ToNumber()->Value();

  v8::Handle<v8::Object> result = QueryTempl->NewInstance();

  result->SetInternalField(SLOT_TYPE, LimitQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());
  result->SetInternalField(SLOT_OPERAND, query);
  result->SetInternalField(SLOT_LIMIT, v8::Uint32::New(limit));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query and returns the next result
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> NextQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::Handle<v8::Object> self = argv.Holder();
  v8::Handle<v8::String> err;

  v8::Handle<v8::Value> q = CheckAndExecuteQuery(argv.Holder(), &err);

  if (q->IsUndefined()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_result_set_t* rs = UnwrapClass<TRI_result_set_t>(q->ToObject());

  if (rs == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute query")));
  }
  else if (rs->hasNext(rs)) {
    TRI_doc_collection_t* collection = rs->_container->_collection;
    TRI_shaper_t* shaper = collection->_shaper;
    TRI_voc_did_t did;
    TRI_shaped_json_t* element = rs->next(rs, &did);
    v8::Handle<v8::Value> object = ObjectShapedJson(collection, did, shaper, element);

    return scope.Close(object);
  }
  else {
    return scope.Close(v8::Undefined());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by example
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> SelectQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> query = argv.Holder();

  // query should be un-executed
  if (QueryExecuted(query)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  v8::Handle<v8::Object> result = QueryTempl->NewInstance();

  result->SetInternalField(SLOT_TYPE, SelectQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());
  result->SetInternalField(SLOT_OPERAND, query);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a query into a fluent interface representation
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ShowQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = argv.Holder();

  string result = StringifyQuery(argv.Holder());

  if (QueryExecuted(self)) {
    result += ".execute()";
  }

  return scope.Close(v8::String::New(result.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips an existing query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> SkipQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> query = argv.Holder();

  // query should be un-executed
  if (QueryExecuted(query)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // first and only argument schould be an integer
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: skip(<number>)")));
  }

  v8::Handle<v8::Value> arg1 = argv[0];

  if (! arg1->IsNumber()) {
    return scope.Close(v8::ThrowException(v8::Exception::TypeError(v8::String::New("<number> must be a number"))));
  }

  double skip = arg1->ToNumber()->Value();

  v8::Handle<v8::Object> result = QueryTempl->NewInstance();

  result->SetInternalField(SLOT_TYPE, SkipQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());
  result->SetInternalField(SLOT_OPERAND, query);
  result->SetInternalField(SLOT_SKIP, v8::Uint32::New(skip));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calls the "printQuery" function
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> PrintQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = argv.Holder();
  v8::Handle<v8::Function> print = v8::Handle<v8::Function>::Cast(self->CreationContext()->Global()->Get(PrintQueryFuncName));

  v8::Handle<v8::Value> args[] = { self };
  print->Call(print, 1, args);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query and returns the result als array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ToArrayQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::Handle<v8::Object> self = argv.Holder();
  v8::Handle<v8::String> err;

  v8::Handle<v8::Value> q = CheckAndExecuteQuery(argv.Holder(), &err);

  if (q->IsUndefined()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_result_set_t* rs = UnwrapClass<TRI_result_set_t>(q->ToObject());

  if (rs == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute query")));
  }
  else {
    v8::Handle<v8::Value> result = ArrayResultSet(rs);

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        TRI_RESULT_SET_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_result_set_t into a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ToStringResultSet (v8::Arguments const& argv) {
  v8::HandleScope scope;

  string name = "[result-set]";

  return scope.Close(v8::String::New(name.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       TRI_VOCBASE_COL_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_vocbase_col_t into a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureGeoIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  if (argv.Length() == 1) {
    v8::String::Utf8Value loc(argv[0]);

    if (*loc == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<location> must be an attribute path")));
    }

    TRI_EnsureGeoIndexSimCollection(sim, *loc);
  }
  else if (argv.Length() == 2) {
    v8::String::Utf8Value lat(argv[0]);
    v8::String::Utf8Value lon(argv[1]);

    if (*lat == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<latitiude> must be an attribute path")));
    }

    if (*lon == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<longitude> must be an attribute path")));
    }

    TRI_EnsureGeoIndex2SimCollection(sim, *lat, *lon);
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: ensureGeoIndex(<latitiude>, <longitude>) or ensureGeoIndex(<location>)")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> NearVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> query = argv.Holder();

  // query should be un-executed
  if (QueryExecuted(query)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // either <location>,<latitiude>,<longitude>,<count> or <latitiude-location>,<latitiude>,<longitude-location>,<longitude>,<count>
  if (argv.Length() < 4) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: near([<location>,<latitiude>,<longitude>|<latitiude-location>,<latitiude>,<longitude-location>,<longitude>],<count>)")));
  }

  if (argv.Length() == 4) {
    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    result->SetInternalField(SLOT_TYPE, NearQueryType);
    result->SetInternalField(SLOT_RESULT_SET, v8::Null());
    result->SetInternalField(SLOT_OPERAND, query);
    result->SetInternalField(SLOT_LOCATION, argv[0]);
    result->SetInternalField(SLOT_LATITUDE, argv[1]);
    result->SetInternalField(SLOT_LONGITUDE, argv[2]);
    result->SetInternalField(SLOT_LIMIT, argv[3]);

    return scope.Close(result);
  }
  else {
    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    result->SetInternalField(SLOT_TYPE, Near2QueryType);
    result->SetInternalField(SLOT_RESULT_SET, v8::Null());
    result->SetInternalField(SLOT_OPERAND, query);
    result->SetInternalField(SLOT_LOC_LATITUDE, argv[0]);
    result->SetInternalField(SLOT_LATITUDE, argv[1]);
    result->SetInternalField(SLOT_LOC_LONGITUDE, argv[2]);
    result->SetInternalField(SLOT_LONGITUDE, argv[3]);
    result->SetInternalField(SLOT_LIMIT, argv[4]);

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the parameters
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParameterVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  // check if we want to change same parameters
  if (0 < argv.Length()) {
    v8::Handle<v8::Value> par = argv[0];

    if (par->IsObject()) {
      v8::Handle<v8::Object> po = par->ToObject();

      TRI_LockCondition(&sim->_journalsCondition);

      TRI_voc_size_t syncAfterObjects = sim->base.base._syncAfterObjects;
      TRI_voc_size_t syncAfterBytes = sim->base.base._syncAfterBytes;
      double syncAfterTime = sim->base.base._syncAfterTime;

      TRI_UnlockCondition(&sim->_journalsCondition);

      bool error;

      // extract sync after objects
      if (po->Has(SyncAfterObjectsKey)) {
        syncAfterObjects = ObjectToDouble(po->Get(SyncAfterObjectsKey), error);

        if (error || syncAfterObjects < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterObjects must be a number")));
        }
      }

      // extract sync after bytes
      if (po->Has(SyncAfterBytesKey)) {
        syncAfterBytes = ObjectToDouble(po->Get(SyncAfterBytesKey), error);

        if (error || syncAfterBytes < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterBytes must be a number")));
        }
      }

      // extract sync after times
      if (po->Has(SyncAfterTimeKey)) {
        syncAfterTime = ObjectToDouble(po->Get(SyncAfterTimeKey), error);

        if (error || syncAfterTime < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterTime must be a non-negative number")));
        }
      }

      // try to write new parameter to file
      TRI_LockCondition(&sim->_journalsCondition);

      sim->base.base._syncAfterObjects = syncAfterObjects;
      sim->base.base._syncAfterBytes = syncAfterBytes;
      sim->base.base._syncAfterTime = syncAfterTime;

      bool ok = TRI_UpdateParameterInfoCollection(&sim->base.base);

      TRI_UnlockCondition(&sim->_journalsCondition);

      if (! ok) {
        return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
      }
    }
  }

  // return the current parameter set
  v8::Handle<v8::Object> result = v8::Object::New();

  if (collection->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_LockCondition(&sim->_journalsCondition);

    TRI_voc_size_t maximalSize = sim->base.base._maximalSize;
    TRI_voc_size_t syncAfterObjects = sim->base.base._syncAfterObjects;
    TRI_voc_size_t syncAfterBytes = sim->base.base._syncAfterBytes;
    double syncAfterTime = sim->base.base._syncAfterTime;

    TRI_UnlockCondition(&sim->_journalsCondition);

    result->Set(SyncAfterObjectsKey, v8::Number::New(syncAfterObjects));
    result->Set(SyncAfterBytesKey, v8::Number::New(syncAfterBytes));
    result->Set(SyncAfterTimeKey, v8::Number::New(syncAfterTime));
    result->Set(JournalSizeKey, v8::Number::New(maximalSize));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> SaveVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: save(<document>)")));
  }

  v8::Handle<v8::Value> val = argv[0];

  TRI_shape_value_t dst;
  bool ok = FillShapeValueJson(collection->_shaper, &dst, val);

  if (! ok) {
    return scope.Close(v8::ThrowException(v8::String::New("<document> cannot be converted into JSON shape")));
  }

  TRI_shaped_json_t shaped;

  shaped._sid = dst._sid;
  shaped._data.length = dst._size;
  shaped._data.data = dst._value;

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->beginWrite(collection);

  TRI_voc_did_t did = collection->create(collection, &shaped);

  collection->endWrite(collection);

  // .............................................................................
  // outside a read transaction
  // .............................................................................

  if (did == 0) {
    string err = "cannot save document: ";
    err += TRI_last_error();

    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  TRI_Free(dst._value);

  char* cidStr = TRI_StringUInt64(collection->base._cid);
  char* didStr = TRI_StringUInt64(did);

  string name = cidStr + string(":") + didStr;

  TRI_FreeString(didStr);
  TRI_FreeString(cidStr);

  return scope.Close(v8::String::New(name.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_vocbase_col_t into a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ToStringVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* collection = UnwrapClass<TRI_vocbase_col_t>(argv.Holder());

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  string name;

  if (collection->_corrupted) {
    name = "[corrupted collection \"" + string(collection->_name) + "\"]";
  }
  else if (collection->_newBorn) {
    name = "[new born collection \"" + string(collection->_name) + "\"]";
  }
  else if (collection->_loaded) {
    name = "[collection \"" + string(collection->_name) + "\"]";
  }
  else {
    name = "[unloaded collection \"" + string(collection->_name) + "\"]";
  }

  return scope.Close(v8::String::New(name.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           TRI_VOCBASE_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_vocbase_t into a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ToStringVocBase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(argv.Holder());

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  string name = "[vocbase at \"" + string(vocbase->_path) + "\"]";

  return scope.Close(v8::String::New(name.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a collection from the vocbase
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetVocBase (v8::Local<v8::String> name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;
  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(info.Holder());

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // convert the JavaScript string to a string
  string key = ObjectToString(name);

  // look up the value if it exists
  TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, key.c_str());

  // if the key is not present return an empty handle as signal
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot load or create collection")));
  }

  // otherwise wrap collection into V8 object
  v8::Handle<v8::Object> result = WrapClass(VocbaseColTempl, const_cast<TRI_vocbase_col_t*>(collection));

  result->SetInternalField(SLOT_TYPE, CollectionQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::Context> InitV8VocBridge (TRI_vocbase_t* vocbase) {
  v8::Handle<v8::ObjectTemplate> rt;

  // create the regular expressions
  if (regcomp(&DocumentIdRegex, "([0-9][0-9]*):([0-9][0-9]*)", REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    exit(EXIT_FAILURE);
  }

  // create the global template
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

  // create the global functions
  InitV8Shell(global);

  // create the context
  v8::Persistent<v8::Context> context = v8::Context::New(0, global);

  if (context.IsEmpty()) {
    printf("cannot initialize V8 engine\n");
    exit(EXIT_FAILURE);
  }

  context->Enter();

  // create the various strings nedded
  CountFuncName = v8::Persistent<v8::String>::New(v8::String::New("count"));
  DocumentFuncName = v8::Persistent<v8::String>::New(v8::String::New("document"));
  EnsureGeoIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureGeoIndex"));
  ExplainFuncName = v8::Persistent<v8::String>::New(v8::String::New("explain"));
  HasNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("hasNext"));
  LimitFuncName = v8::Persistent<v8::String>::New(v8::String::New("limit"));
  NearFuncName = v8::Persistent<v8::String>::New(v8::String::New("near"));
  NextFuncName = v8::Persistent<v8::String>::New(v8::String::New("next"));
  OutputFuncName = v8::Persistent<v8::String>::New(v8::String::New("output"));
  ParameterFuncName = v8::Persistent<v8::String>::New(v8::String::New("parameter"));
  PrintFuncName = v8::Persistent<v8::String>::New(v8::String::New("print"));
  PrintQueryFuncName = v8::Persistent<v8::String>::New(v8::String::New("printQuery"));
  SaveFuncName = v8::Persistent<v8::String>::New(v8::String::New("save"));
  SelectFuncName = v8::Persistent<v8::String>::New(v8::String::New("select"));
  ShowFuncName = v8::Persistent<v8::String>::New(v8::String::New("show"));
  SkipFuncName = v8::Persistent<v8::String>::New(v8::String::New("skip"));
  ToArrayFuncName = v8::Persistent<v8::String>::New(v8::String::New("toArray"));
  ToStringFuncName = v8::Persistent<v8::String>::New(v8::String::New("toString"));

  CollectionQueryType = v8::Persistent<v8::String>::New(v8::String::New("collection"));
  DocumentQueryType = v8::Persistent<v8::String>::New(v8::String::New("document"));
  LimitQueryType = v8::Persistent<v8::String>::New(v8::String::New("limit"));
  Near2QueryType = v8::Persistent<v8::String>::New(v8::String::New("near2"));
  NearQueryType = v8::Persistent<v8::String>::New(v8::String::New("near"));
  SelectQueryType = v8::Persistent<v8::String>::New(v8::String::New("select"));
  SkipQueryType = v8::Persistent<v8::String>::New(v8::String::New("skip"));

  DidKey = v8::Persistent<v8::String>::New(v8::String::New("_id"));
  JournalSizeKey = v8::Persistent<v8::String>::New(v8::String::New("journalSize"));
  SyncAfterBytesKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterBytes"));
  SyncAfterObjectsKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterObjects"));
  SyncAfterTimeKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterTime"));

  // generate the TRI_vocbase_t template
  rt = v8::ObjectTemplate::New();

  rt->SetInternalFieldCount(1);

  rt->Set(PrintFuncName, v8::FunctionTemplate::New(PrintUsingToString));
  rt->Set(ToStringFuncName, v8::FunctionTemplate::New(ToStringVocBase));
  rt->SetNamedPropertyHandler(MapGetVocBase);

  VocBaseTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // generate the TRI_vocbase_col_t template
  rt = v8::ObjectTemplate::New();

  rt->SetInternalFieldCount(5);

  rt->Set(CountFuncName, v8::FunctionTemplate::New(CountQuery));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(DocumentQuery));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(EnsureGeoIndexVocbaseCol));
  rt->Set(ExplainFuncName, v8::FunctionTemplate::New(ExplainQuery));
  rt->Set(LimitFuncName, v8::FunctionTemplate::New(LimitQuery));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(NearVocbaseCol));
  rt->Set(ParameterFuncName, v8::FunctionTemplate::New(ParameterVocbaseCol));
  rt->Set(PrintFuncName, v8::FunctionTemplate::New(PrintUsingToString));
  rt->Set(SaveFuncName, v8::FunctionTemplate::New(SaveVocbaseCol));
  rt->Set(SelectFuncName, v8::FunctionTemplate::New(SelectQuery));
  rt->Set(SkipFuncName, v8::FunctionTemplate::New(SkipQuery));
  rt->Set(ToArrayFuncName, v8::FunctionTemplate::New(ToArrayQuery));
  rt->Set(ToStringFuncName, v8::FunctionTemplate::New(ToStringVocbaseCol));

  VocbaseColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // generate the query template
  rt = v8::ObjectTemplate::New();

  rt->SetInternalFieldCount(SLOT_END);

  rt->Set(CountFuncName, v8::FunctionTemplate::New(CountQuery));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(DocumentQuery));
  rt->Set(ExplainFuncName, v8::FunctionTemplate::New(ExplainQuery));
  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(HasNextQuery));
  rt->Set(LimitFuncName, v8::FunctionTemplate::New(LimitQuery));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(NextQuery));
  rt->Set(PrintFuncName, v8::FunctionTemplate::New(PrintQuery));
  rt->Set(SelectFuncName, v8::FunctionTemplate::New(SelectQuery));
  rt->Set(ShowFuncName, v8::FunctionTemplate::New(ShowQuery));
  rt->Set(SkipFuncName, v8::FunctionTemplate::New(SkipQuery));
  rt->Set(ToArrayFuncName, v8::FunctionTemplate::New(ToArrayQuery));

  QueryTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // generate the result-set template
  rt = v8::ObjectTemplate::New();

  rt->SetInternalFieldCount(1);

  rt->Set(ToStringFuncName, v8::FunctionTemplate::New(ToStringResultSet));

  ResultSetTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // create the global variables
  context->Global()->Set(v8::String::New("db"),
                         WrapClass(VocBaseTempl, vocbase),
                         v8::ReadOnly);

  // and return the context
  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
