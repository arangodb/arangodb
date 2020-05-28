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

#include "v8-user-structures.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Utf8Helper.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Basics/json.h"
#include "Basics/memory.h"
#include "Basics/tri-strings.h"
#include "Basics/tryEmplaceHelper.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

int TRI_CompareValuesJson(TRI_json_t const* lhs, TRI_json_t const* rhs, bool useUTF8 = true);

static TRI_json_t* MergeRecursive(TRI_json_t const* lhs, TRI_json_t const* rhs,
                                  bool nullMeansRemove, bool mergeObjects) {
  TRI_ASSERT(lhs != nullptr);

  std::unique_ptr<TRI_json_t> result(TRI_CopyJson(lhs));

  if (result == nullptr) {
    return nullptr;
  }

  auto r = result.get();  // shortcut variable

  size_t const n = TRI_LengthVector(&rhs->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    // enumerate all the replacement values
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&rhs->_value._objects, i));
    auto value =
        static_cast<TRI_json_t const*>(TRI_AtVector(&rhs->_value._objects, i + 1));

    if (value->_type == TRI_JSON_NULL && nullMeansRemove) {
      // replacement value is a null and we don't want to store nulls => delete
      // attribute from the result
      TRI_DeleteObjectJson(r, key->_value._string.data);
    } else {
      // replacement value is not a null or we want to store nulls
      TRI_json_t const* lhsValue = TRI_LookupObjectJson(lhs, key->_value._string.data);

      if (lhsValue == nullptr) {
        // existing array does not have the attribute => append new attribute
        if (value->_type == TRI_JSON_OBJECT && nullMeansRemove) {
          TRI_json_t empty;
          TRI_InitObjectJson(&empty);
          TRI_json_t* merged = MergeRecursive(&empty, value, nullMeansRemove, mergeObjects);

          if (merged == nullptr) {
            return nullptr;
          }
          TRI_Insert3ObjectJson(r, key->_value._string.data, merged);
        } else {
          TRI_json_t* copy = TRI_CopyJson(value);

          if (copy == nullptr) {
            return nullptr;
          }

          TRI_Insert3ObjectJson(r, key->_value._string.data, copy);
        }
      } else {
        // existing array already has the attribute => replace attribute
        if (lhsValue->_type == TRI_JSON_OBJECT && value->_type == TRI_JSON_OBJECT && mergeObjects) {
          TRI_json_t* merged =
              MergeRecursive(lhsValue, value, nullMeansRemove, mergeObjects);
          if (merged == nullptr) {
            return nullptr;
          }
          TRI_ReplaceObjectJson(r, key->_value._string.data, merged);
          TRI_FreeJson(merged);
        } else {
          TRI_ReplaceObjectJson(r, key->_value._string.data, value);
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
  std::unique_ptr<TRI_json_t> result(TRI_CreateArrayJson());

  if (result == nullptr) {
    return nullptr;
  }

  size_t const n = TRI_LengthVector(&array->_value._objects);

  TRI_json_t const* last = nullptr;
  for (size_t i = 0; i < n; ++i) {
    auto p = static_cast<TRI_json_t const*>(TRI_AtVector(&array->_value._objects, i));

    // don't push value if it is the same as the last value
    if (last == nullptr || TRI_CompareValuesJson(p, last, false) != 0) {
      int res = TRI_PushBackArrayJson(result.get(), p);

      if (res != TRI_ERROR_NO_ERROR) {
        return nullptr;
      }

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
    qsort(TRI_BeginVector(&array->_value._objects), n, sizeof(TRI_json_t), &CompareJson);
  }

  return array;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two arrays of array keys, sort them and return a combined array
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* GetMergedKeyArray(TRI_json_t const* lhs, TRI_json_t const* rhs) {
  TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);

  size_t n = TRI_LengthVector(&lhs->_value._objects) +
             TRI_LengthVector(&rhs->_value._objects);

  std::unique_ptr<TRI_json_t> keys(TRI_CreateArrayJson(n));

  if (keys == nullptr) {
    return nullptr;
  }

  if (TRI_CapacityVector(&(keys.get()->_value._objects)) < n) {
    return nullptr;
  }

  n = TRI_LengthVector(&lhs->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&lhs->_value._objects, i));

    TRI_ASSERT(TRI_IsStringJson(key));
    int res = TRI_PushBackArrayJson(keys.get(), key);

    if (res != TRI_ERROR_NO_ERROR) {
      return nullptr;
    }
  }

  n = TRI_LengthVector(&rhs->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&rhs->_value._objects, i));

    TRI_ASSERT(TRI_IsStringJson(key));
    int res = TRI_PushBackArrayJson(keys.get(), key);

    if (res != TRI_ERROR_NO_ERROR) {
      return nullptr;
    }
  }

  // sort the key array in place
  SortArrayJson(keys.get());

  // array is now sorted
  return UniquifyArrayJson(keys.get());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two json values
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareValuesJson(TRI_json_t const* lhs, TRI_json_t const* rhs, bool useUTF8) {
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
        res = TRI_compare_utf8(lhs->_value._string.data, nl, rhs->_value._string.data, nr);
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
        auto lhsValue = (i >= nl) ? nullptr
                                  : static_cast<TRI_json_t const*>(
                                        TRI_AtVector(&lhs->_value._objects, i));
        auto rhsValue = (i >= nr) ? nullptr
                                  : static_cast<TRI_json_t const*>(
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

      if (keys == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      auto json = keys.get();
      size_t const n = TRI_LengthVector(&json->_value._objects);

      for (size_t i = 0; i < n; ++i) {
        auto keyElement =
            static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
        TRI_ASSERT(TRI_IsStringJson(keyElement));

        TRI_json_t const* lhsValue =
            TRI_LookupObjectJson(lhs, keyElement->_value._string.data);  // may be NULL
        TRI_json_t const* rhsValue =
            TRI_LookupObjectJson(rhs, keyElement->_value._string.data);  // may be NULL

        int result = TRI_CompareValuesJson(lhsValue, rhsValue, useUTF8);

        if (result != 0) {
          return result;
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

static TRI_json_t* TRI_MergeJson(TRI_json_t const* lhs, TRI_json_t const* rhs,
                                 bool nullMeansRemove, bool mergeObjects) {
  TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);

  return MergeRecursive(lhs, rhs, nullMeansRemove, mergeObjects);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ObjectJson(v8::Isolate* isolate, TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the keys of a TRI_json_t* object into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_KeysJson(v8::Isolate* isolate, TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the values of a TRI_json_t* object into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ValuesJson(v8::Isolate* isolate, TRI_json_t const*);

/// @brief converts a TRI_json_t NULL into a V8 object
static inline v8::Handle<v8::Value> ObjectJsonNull(v8::Isolate* isolate,
                                                   TRI_json_t const* json) {
  return v8::Null(isolate);
}

/// @brief converts a TRI_json_t BOOLEAN into a V8 object
static inline v8::Handle<v8::Value> ObjectJsonBoolean(v8::Isolate* isolate,
                                                      TRI_json_t const* json) {
  return v8::Boolean::New(isolate, json->_value._boolean);
}

/// @brief converts a TRI_json_t NUMBER into a V8 object
static inline v8::Handle<v8::Value> ObjectJsonNumber(v8::Isolate* isolate,
                                                     TRI_json_t const* json) {
  return v8::Number::New(isolate, json->_value._number);
}

/// @brief converts a TRI_json_t STRING into a V8 object
static inline v8::Handle<v8::Value> ObjectJsonString(v8::Isolate* isolate,
                                                     TRI_json_t const* json) {
  return TRI_V8_PAIR_STRING(isolate, json->_value._string.data,
                            json->_value._string.length - 1);
}

/// @brief converts a TRI_json_t OBJECT into a V8 object
static v8::Handle<v8::Value> ObjectJsonObject(v8::Isolate* isolate, TRI_json_t const* json) {
  v8::Handle<v8::Object> object = v8::Object::New(isolate);
  auto context = TRI_IGETC;
  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  size_t const n = TRI_LengthVector(&json->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    TRI_json_t const* key =
        static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, i));

    if (!TRI_IsStringJson(key)) {
      continue;
    }

    TRI_json_t const* element =
        static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, i + 1));

    auto val = TRI_ObjectJson(isolate, element);
    if (!val.IsEmpty()) {
      auto k = TRI_V8_PAIR_STRING(isolate, key->_value._string.data,
                                  key->_value._string.length - 1);
      if (!k.IsEmpty()) {
        object->Set(context, TRI_V8_PAIR_STRING(isolate, key->_value._string.data,
                                       key->_value._string.length - 1),
                    val).FromMaybe(false);
      }
    }
  }

  return object;
}

/// @brief converts a TRI_json_t ARRAY into a V8 object
static v8::Handle<v8::Value> ObjectJsonArray(v8::Isolate* isolate, TRI_json_t const* json) {
  uint32_t const n = static_cast<uint32_t>(TRI_LengthArrayJson(json));
  auto context = TRI_IGETC;
  v8::Handle<v8::Array> object = v8::Array::New(isolate, static_cast<int>(n));

  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  uint32_t j = 0;
  for (uint32_t i = 0; i < n; ++i) {
    TRI_json_t const* element =
        static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, i));
    v8::Handle<v8::Value> val = TRI_ObjectJson(isolate, element);

    if (!val.IsEmpty()) {
      object->Set(context, j++, val).FromMaybe(false);
    }
  }

  return object;
}

/// @brief extracts keys or values from a TRI_json_t* object
static v8::Handle<v8::Value> ExtractObject(v8::Isolate* isolate,
                                           TRI_json_t const* json, size_t offset) {
  v8::EscapableHandleScope scope(isolate);
  auto context = TRI_IGETC;
  if (json == nullptr || json->_type != TRI_JSON_OBJECT) {
    return scope.Escape<v8::Value>(v8::Undefined(isolate));
  }

  size_t const n = TRI_LengthVector(&json->_value._objects);

  v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n / 2));
  uint32_t count = 0;

  for (size_t i = offset; i < n; i += 2) {
    TRI_json_t const* value =
        static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));

    if (value != nullptr) {
      result->Set(context, count++, TRI_ObjectJson(isolate, value)).FromMaybe(false);
    }
  }

  return scope.Escape<v8::Value>(result);
}

/// @brief returns the keys of a TRI_json_t* object into a V8 array
v8::Handle<v8::Value> TRI_KeysJson(v8::Isolate* isolate, TRI_json_t const* json) {
  return ExtractObject(isolate, json, 0);
}

/// @brief returns the values of a TRI_json_t* object into a V8 array
v8::Handle<v8::Value> TRI_ValuesJson(v8::Isolate* isolate, TRI_json_t const* json) {
  return ExtractObject(isolate, json, 1);
}

/// @brief converts a TRI_json_t into a V8 object
v8::Handle<v8::Value> TRI_ObjectJson(v8::Isolate* isolate, TRI_json_t const* json) {
  if (json == nullptr) {
    return v8::Undefined(isolate);
  }

  switch (json->_type) {
    case TRI_JSON_NULL:
      return ObjectJsonNull(isolate, json);

    case TRI_JSON_BOOLEAN:
      return ObjectJsonBoolean(isolate, json);

    case TRI_JSON_NUMBER:
      return ObjectJsonNumber(isolate, json);

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE:
      return ObjectJsonString(isolate, json);

    case TRI_JSON_OBJECT:
      return ObjectJsonObject(isolate, json);

    case TRI_JSON_ARRAY:
      return ObjectJsonArray(isolate, json);

    case TRI_JSON_UNUSED: {
    }
  }

  return v8::Undefined(isolate);
}

/// @brief convert a V8 value to a TRI_json_t value
static int ObjectToJson(v8::Isolate* isolate, TRI_json_t* result,
                        v8::Handle<v8::Value> const parameter, std::set<int>& seenHashes,
                        std::vector<v8::Handle<v8::Object>>& seenObjects) {
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  if (parameter->IsNull()) {
    TRI_InitNullJson(result);
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsBoolean()) {
    v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean(isolate);
    TRI_InitBooleanJson(result, booleanParameter->Value());
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsNumber()) {
    v8::Handle<v8::Number> numberParameter =
        parameter->ToNumber(context).FromMaybe(v8::Local<v8::Number>());
    TRI_InitNumberJson(result, numberParameter->Value());
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsString()) {
    v8::Handle<v8::String> stringParameter =
        parameter->ToString(context).FromMaybe(v8::Local<v8::String>());
    TRI_Utf8ValueNFC str(isolate, stringParameter);

    if (*str == nullptr) {
      TRI_InitNullJson(result);
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // this passes ownership for the utf8 string to the JSON object
    TRI_InitStringJson(result, str.steal(), str.length());
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsArray()) {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(parameter);
    uint32_t const n = array->Length();

    // allocate the result array in one go
    TRI_InitArrayJson(result, static_cast<size_t>(n));
    int res = TRI_ReserveVector(&result->_value._objects, static_cast<size_t>(n));

    if (res != TRI_ERROR_NO_ERROR) {
      // result array could not be allocated
      TRI_InitNullJson(result);
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < n; ++i) {
      // get address of next element
      auto next = static_cast<TRI_json_t*>(TRI_NextVector(&result->_value._objects));
      // the reserve call above made sure we could not have run out of memory
      TRI_ASSERT(next != nullptr);

      res = ObjectToJson(isolate, next, array->Get(context, i).FromMaybe(v8::Local<v8::Value>()), seenHashes, seenObjects);

      if (res != TRI_ERROR_NO_ERROR) {
        // to mimic behavior of previous ArangoDB versions, we need to silently
        // ignore this error
        // now return the element to the vector
        TRI_ReturnVector(&result->_value._objects);

        // a better solution would be:
        // initialize the element at position, otherwise later cleanups may
        // peek into uninitialized memory
        // TRI_InitNullJson(next);
        // return res;
      }
    }

    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsObject()) {
    if (parameter->IsBooleanObject()) {
      TRI_InitBooleanJson(result, v8::Handle<v8::BooleanObject>::Cast(parameter)
                          ->BooleanValue(isolate));
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsNumberObject()) {
      TRI_InitNumberJson(result, v8::Handle<v8::NumberObject>::Cast(parameter)
                                     ->NumberValue(context)
                                     .FromMaybe(0.0));
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsStringObject()) {
      v8::Handle<v8::String> stringParameter(
          parameter->ToString(context).FromMaybe(v8::Local<v8::String>()));
      TRI_Utf8ValueNFC str(isolate, stringParameter);

      if (*str == nullptr) {
        TRI_InitNullJson(result);
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      // this passes ownership for the utf8 string to the JSON object
      TRI_InitStringJson(result, str.steal(), str.length());
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsRegExp() || parameter->IsFunction() || parameter->IsExternal()) {
      TRI_InitNullJson(result);
      return TRI_ERROR_BAD_PARAMETER;
    }

    v8::Handle<v8::Object> o =
        parameter->ToObject(context).FromMaybe(v8::Local<v8::Object>());

    // first check if the object has a "toJSON" function
    v8::Handle<v8::String> toJsonString = TRI_V8_PAIR_STRING(isolate, "toJSON", 6);
    if (TRI_HasProperty(context, isolate, o, toJsonString)) {
      // call it if yes
      v8::Handle<v8::Value> func =
          o->Get(context, toJsonString).FromMaybe(v8::Local<v8::Value>());
      if (func->IsFunction()) {
        v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(func);

        v8::Handle<v8::Value> args;
        v8::Handle<v8::Value> converted = toJson->Call(context, o, 0, &args).FromMaybe(v8::Local<v8::Value>());

        if (!converted.IsEmpty()) {
          // return whatever toJSON returned
          TRI_Utf8ValueNFC str(isolate, converted->ToString(context).FromMaybe(
                                            v8::Local<v8::String>()));

          if (*str == nullptr) {
            TRI_InitNullJson(result);
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          // this passes ownership for the utf8 string to the JSON object
          TRI_InitStringJson(result, str.steal(), str.length());
          return TRI_ERROR_NO_ERROR;
        }
      }

      // intentionally falls through
    }

    int hashval = o->GetIdentityHash();

    if (seenHashes.find(hashval) != seenHashes.end()) {
      // LOG_TOPIC("a6d3e", TRACE, arangodb::Logger::FIXME) << "found hash " << hashval;

      for (auto it : seenObjects) {
        if (parameter->StrictEquals(it)) {
          // object is recursive
          TRI_InitNullJson(result);
          return TRI_ERROR_BAD_PARAMETER;
        }
      }
    } else {
      seenHashes.emplace(hashval);
    }

    seenObjects.emplace_back(o);

    v8::Handle<v8::Array> names = o->GetOwnPropertyNames(context).FromMaybe(v8::Local<v8::Array>());
    uint32_t const n = names->Length();

    // allocate the result object buffer in one go
    TRI_InitObjectJson(result, static_cast<size_t>(n));
    int res = TRI_ReserveVector(&result->_value._objects,
                                static_cast<size_t>(n * 2));  // key + value

    if (res != TRI_ERROR_NO_ERROR) {
      // result object buffer could not be allocated
      TRI_InitNullJson(result);
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < n; ++i) {
      // process attribute name
      v8::Handle<v8::Value> key = names->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      TRI_Utf8ValueNFC str(isolate, key);

      if (*str == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      TRI_json_t* next =
          static_cast<TRI_json_t*>(TRI_NextVector(&result->_value._objects));
      // the reserve call above made sure we could not have run out of memory
      TRI_ASSERT(next != nullptr);

      // this passes ownership for the utf8 string to the JSON object
      char* attributeName = str.steal();
      TRI_InitStringJson(next, attributeName, str.length());

      // process attribute value
      next = static_cast<TRI_json_t*>(TRI_NextVector(&result->_value._objects));
      // the reserve call above made sure we could not have run out of memory
      TRI_ASSERT(next != nullptr);

      res = ObjectToJson(isolate, next, o->Get(context, key).FromMaybe(v8::Local<v8::Value>()), seenHashes, seenObjects);

      if (res != TRI_ERROR_NO_ERROR) {
        // to mimic behavior of previous ArangoDB versions, we need to silently
        // ignore this error
        // now free the attributeName string and return the elements to the
        // vector
        TRI_FreeString(attributeName);
        TRI_ReturnVector(&result->_value._objects);
        TRI_ReturnVector(&result->_value._objects);

        // a better solution would be:
        // initialize the element at position, otherwise later cleanups may
        // peek into uninitialized memory
        // TRI_InitNullJson(next);
        // return res;
      }
    }

    seenObjects.pop_back();

    return TRI_ERROR_NO_ERROR;
  }

  TRI_InitNullJson(result);
  return TRI_ERROR_BAD_PARAMETER;
}

/// @brief convert a V8 value to a json_t value
TRI_json_t* TRI_ObjectToJson(v8::Isolate* isolate, v8::Handle<v8::Value> const parameter) {
  TRI_json_t* json = TRI_CreateNullJson();

  if (json == nullptr) {
    return nullptr;
  }

  std::set<int> seenHashes;
  std::vector<v8::Handle<v8::Object>> seenObjects;
  int res = ObjectToJson(isolate, json, parameter, seenHashes, seenObjects);

  if (res != TRI_ERROR_NO_ERROR) {
    // some processing error occurred
    TRI_FreeJson(json);
    return nullptr;
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a json_t value
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ObjectToJson(v8::Isolate*, v8::Handle<v8::Value> const);

struct KeySpaceElement {
  KeySpaceElement() = delete;

  KeySpaceElement(char const* k, size_t length, TRI_json_t* json)
      : key(nullptr), json(json) {
    key = TRI_DuplicateString(k, length);
    if (key == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  ~KeySpaceElement() {
    if (key != nullptr) {
      TRI_FreeString(key);
    }
    if (json != nullptr) {
      TRI_FreeJson(json);
    }
  }

  void setValue(TRI_json_t* value) {
    if (json != nullptr) {
      TRI_FreeJson(json);
      json = nullptr;
    }
    json = value;
  }

  char* key;
  TRI_json_t* json;
};

class KeySpace {
 public:
  explicit KeySpace(uint32_t initialSize) : _lock() {}

  ~KeySpace() {
    for (auto& it : _hash) {
      auto element = it.second;

      if (element != nullptr) {
        delete element;
      }
    }
  }

  uint32_t keyspaceCount() {
    READ_LOCKER(readLocker, _lock);
    return static_cast<uint32_t>(_hash.size());
  }

  uint32_t keyspaceCount(std::string const& prefix) {
    uint32_t count = 0;
    READ_LOCKER(readLocker, _lock);

    for (auto const& it : _hash) {
      auto data = it.second;

      if (data != nullptr) {
        if (TRI_IsPrefixString(data->key, prefix.c_str())) {
          ++count;
        }
      }
    }

    return count;
  }

  v8::Handle<v8::Value> keyspaceRemove(v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);

    WRITE_LOCKER(writeLocker, _lock);
    uint32_t deleted = 0;

    for (auto& it : _hash) {
      auto element = it.second;

      if (element != nullptr) {
        delete element;
        ++deleted;
      }
    }
    _hash.clear();

    return scope.Escape<v8::Value>(v8::Number::New(isolate, static_cast<int>(deleted)));
  }

  v8::Handle<v8::Value> keyspaceRemove(v8::Isolate* isolate, std::string const& prefix) {
    v8::EscapableHandleScope scope(isolate);
    WRITE_LOCKER(writeLocker, _lock);

    uint32_t deleted = 0;

    for (auto it = _hash.begin(); it != _hash.end(); /* no hoisting */) {
      auto element = (*it).second;

      if (element != nullptr) {
        if (TRI_IsPrefixString(element->key, prefix.c_str())) {
          it = _hash.erase(it);
          delete element;
          ++deleted;
          continue;
        }
      }
      ++it;
    }

    return scope.Escape<v8::Value>(v8::Number::New(isolate, static_cast<int>(deleted)));
  }

  v8::Handle<v8::Value> keyspaceKeys(v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);
    auto context = TRI_IGETC;
    v8::Handle<v8::Array> result;
    {
      READ_LOCKER(readLocker, _lock);

      uint32_t count = 0;
      result = v8::Array::New(isolate, static_cast<int>(_hash.size()));

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          result->Set(context,
                      count++,
                      TRI_V8_PAIR_STRING(isolate,
                                         element->key,
                                         strlen(element->key))).FromMaybe(false);
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  v8::Handle<v8::Value> keyspaceKeys(v8::Isolate* isolate, std::string const& prefix) {
    v8::EscapableHandleScope scope(isolate);
    auto context = TRI_IGETC;
    v8::Handle<v8::Array> result;
    {
      READ_LOCKER(readLocker, _lock);

      uint32_t count = 0;
      result = v8::Array::New(isolate);

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          if (TRI_IsPrefixString(element->key, prefix.c_str())) {
            result->Set(context,
                        count++,
                        TRI_V8_PAIR_STRING(isolate, element->key,
                                           strlen(element->key))).FromMaybe(false);
          }
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  v8::Handle<v8::Value> keyspaceGet(v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);
    auto context = TRI_IGETC;
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    {
      READ_LOCKER(readLocker, _lock);

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          result->Set(context, TRI_V8_PAIR_STRING(isolate, element->key, strlen(element->key)),
                      TRI_ObjectJson(isolate, element->json)).FromMaybe(false);
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  v8::Handle<v8::Value> keyspaceGet(v8::Isolate* isolate, std::string const& prefix) {
    v8::EscapableHandleScope scope(isolate);
    auto context = TRI_IGETC;
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    {
      READ_LOCKER(readLocker, _lock);

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          if (TRI_IsPrefixString(element->key, prefix.c_str())) {
            result->Set(context, TRI_V8_PAIR_STRING(isolate, element->key, strlen(element->key)),
                        TRI_ObjectJson(isolate, element->json)).FromMaybe(false);
          }
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  bool keyCount(std::string const& key, uint32_t& result) {
    READ_LOCKER(readLocker, _lock);

    auto it = _hash.find(key);

    if (it != _hash.end()) {
      TRI_json_t const* value = (*it).second->json;

      if (TRI_IsArrayJson(value)) {
        result = static_cast<uint32_t>(TRI_LengthVector(&value->_value._objects));
        return true;
      }
      if (TRI_IsObjectJson(value)) {
        result = static_cast<uint32_t>(TRI_LengthVector(&value->_value._objects) / 2);
        return true;
      }
    }

    result = 0;
    return false;
  }

  v8::Handle<v8::Value> keyGet(v8::Isolate* isolate, std::string const& key) {
    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        result = TRI_ObjectJson(isolate, (*it).second->json);
      }
    }

    return result;
  }

  bool keySet(v8::Isolate* isolate, std::string const& key,
              v8::Handle<v8::Value> const& value, bool replace) {
    // do not get memory under the lock
    auto element = std::make_unique<KeySpaceElement>(key.c_str(), key.size(),
                                       TRI_ObjectToJson(isolate, value));
      WRITE_LOCKER(writeLocker, _lock);

    auto [it, emplaced] = _hash.try_emplace(key, element.get());

    if (replace && !emplaced) {
        // delete previous entry
      delete it->second;
      it->second =  element.get();
      emplaced=true;
      }

    if (emplaced) {
      element.release();
    }

    return emplaced;
  }


  bool keySet(std::string const& key, double val) {
    // do not get memory under the lock
    TRI_json_t* json = TRI_CreateNumberJson(val);
    if (json == nullptr) {
      // OOM
      return false;
    }
    auto element = std::make_unique<KeySpaceElement>(key.c_str(), key.size(), json);

    {
      WRITE_LOCKER(writeLocker, _lock);
      auto [it, emplaced] = _hash.try_emplace(key, element.get());

      if (emplaced) {
        element.release();
      } else {
        it->second->json->_value._number = val;
        emplaced = true;
      }

      return emplaced;
    }
  }

  int keyCas(v8::Isolate* isolate, std::string const& key, v8::Handle<v8::Value> const& value,
             v8::Handle<v8::Value> const& compare, bool& match) {
    // do not get memory under the lock
    auto element = std::make_unique<KeySpaceElement>(key.c_str(), key.size(),
                                       TRI_ObjectToJson(isolate, value));

    WRITE_LOCKER(writeLocker, _lock);

    auto [it, emplaced] = _hash.try_emplace(key, element.get());
    if (emplaced) {
      // no object saved yet
      match = true;
      element.release();
      return TRI_ERROR_NO_ERROR;
    }


    if (compare->IsUndefined()) {
      // other object saved, but we compare it with nothing => no match
      match = false;
      return TRI_ERROR_NO_ERROR;
    }

    TRI_json_t* other = TRI_ObjectToJson(isolate, compare);
    if (other == nullptr) {
      match = false;
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    KeySpaceElement* found = it->second;
    int res = TRI_CompareValuesJson(found->json, other);
    TRI_FreeJson(other);

    if (res != 0) {
      match = false;
    } else {
        delete found;
      found = element.release();
      match = true;
    }

    return TRI_ERROR_NO_ERROR;
  }

  bool keyRemove(std::string const& key) {
    KeySpaceElement* found = nullptr;

    {
      WRITE_LOCKER(writeLocker, _lock);

      auto it = _hash.find(key);
      if (it != _hash.end()) {
        found = (*it).second;
        _hash.erase(it);
      }
    }

    if (found != nullptr) {
      delete found;
      return true;
    }

    return false;
  }

  bool keyExists(std::string const& key) {
    READ_LOCKER(readLocker, _lock);

    return _hash.find(key) != _hash.end();
  }

  int keyIncr(std::string const& key, double value, double& result) {
    WRITE_LOCKER(writeLocker, _lock);

    auto [found, emplaced] = _hash.try_emplace(
      key,
      arangodb::lazyConstruct([&]{
          return new KeySpaceElement(key.c_str(), key.size(), TRI_CreateNumberJson(value));
      })
    );

    if (emplaced) {
      result = value;
    } else {
      TRI_json_t* current = found->second->json;
      if (!TRI_IsNumberJson(current)) {
        return TRI_ERROR_ILLEGAL_NUMBER;
      }
      result = current->_value._number += value;
    }

    return TRI_ERROR_NO_ERROR;
  }

  int keyPush(v8::Isolate* isolate, std::string const& key,
              v8::Handle<v8::Value> const& value) {
    WRITE_LOCKER(writeLocker, _lock);

    KeySpaceElement* found = nullptr;
    auto it = _hash.find(key);
    if (it != _hash.end()) {
      found = (*it).second;
    }

    if (found == nullptr) {
      TRI_json_t* list = TRI_CreateArrayJson(1);

      if (list == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      if (TRI_PushBack3ArrayJson(list, TRI_ObjectToJson(isolate, value)) != TRI_ERROR_NO_ERROR) {
        TRI_FreeJson(list);
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      auto element = new KeySpaceElement(key.c_str(), key.size(), list);
      _hash.try_emplace(key, element);
    } else {
      TRI_json_t* current = found->json;

      if (!TRI_IsArrayJson(current)) {
        return TRI_ERROR_INTERNAL;
      }

      if (TRI_PushBack3ArrayJson(current, TRI_ObjectToJson(isolate, value)) != TRI_ERROR_NO_ERROR) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

    return TRI_ERROR_NO_ERROR;
  }

  void keyPop(v8::FunctionCallbackInfo<v8::Value> const& args, std::string const& key) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);
    WRITE_LOCKER(writeLocker, _lock);

    KeySpaceElement* found = nullptr;
    auto it = _hash.find(key);
    if (it != _hash.end()) {
      found = (*it).second;
    }

    if (found == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    // cppcheck-suppress nullPointer ; cannot get here if found is nullptr
    TRI_json_t* current = found->json;

    if (!TRI_IsArrayJson(current)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    size_t const n = TRI_LengthVector(&current->_value._objects);

    if (n == 0) {
      TRI_V8_RETURN_UNDEFINED();
    }

    TRI_json_t* item =
        static_cast<TRI_json_t*>(TRI_AtVector(&current->_value._objects, n - 1));
    // hack: decrease the vector size
    TRI_SetLengthVector(&current->_value._objects,
                        TRI_LengthVector(&current->_value._objects) - 1);

    v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, item);
    TRI_DestroyJson(item);

    TRI_V8_RETURN(result);
  }

  void keyTransfer(v8::FunctionCallbackInfo<v8::Value> const& args,
                   std::string const& keyFrom, std::string const& keyTo) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    WRITE_LOCKER(writeLocker, _lock);

    KeySpaceElement* source = nullptr;
    auto it = _hash.find(keyFrom);
    if (it != _hash.end()) {
      source = (*it).second;
    }

    if (source == nullptr) {
      TRI_V8_RETURN_UNDEFINED();
    }

    TRI_json_t* current = source->json;

    if (!TRI_IsArrayJson(current)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    size_t const n = TRI_LengthVector(&source->json->_value._objects);

    if (n == 0) {
      TRI_V8_RETURN_UNDEFINED();
    }

    TRI_json_t* sourceItem =
        static_cast<TRI_json_t*>(TRI_AtVector(&source->json->_value._objects, n - 1));

    KeySpaceElement* dest = nullptr;
    auto it2 = _hash.find(keyTo);
    if (it2 != _hash.end()) {
      dest = (*it2).second;
    }

    if (dest == nullptr) {
      TRI_json_t* list = TRI_CreateArrayJson(1);

      if (list == nullptr) {
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      TRI_PushBack2ArrayJson(list, sourceItem);

      try {
        auto element = std::make_unique<KeySpaceElement>(keyTo.c_str(), keyTo.size(), list);
        _hash.try_emplace(keyTo, element.get());
        element.release();
        // hack: decrease the vector size
        TRI_SetLengthVector(&current->_value._objects,
                            TRI_LengthVector(&current->_value._objects) - 1);

        TRI_V8_RETURN(TRI_ObjectJson(isolate, sourceItem));
      } catch (...) {
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }
    }

    TRI_ASSERT(dest != nullptr);

    // cppcheck-suppress *
    if (!TRI_IsArrayJson(dest->json)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    // cppcheck-suppress *
    TRI_PushBack2ArrayJson(dest->json, sourceItem);

    // hack: decrease the vector size
    TRI_SetLengthVector(&current->_value._objects,
                        TRI_LengthVector(&current->_value._objects) - 1);

    TRI_V8_RETURN(TRI_ObjectJson(isolate, sourceItem));
  }

  v8::Handle<v8::Value> keyKeys(v8::Isolate* isolate, std::string const& key) {
    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        result = TRI_KeysJson(isolate, (*it).second->json);
      }
    }

    return result;
  }

  v8::Handle<v8::Value> keyValues(v8::Isolate* isolate, std::string const& key) {
    v8::EscapableHandleScope scope(isolate);
    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        result = TRI_ValuesJson(isolate, (*it).second->json);
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  void keyGetAt(v8::FunctionCallbackInfo<v8::Value> const& args,
                std::string const& key, int64_t index) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        KeySpaceElement* found = (*it).second;
        if (!TRI_IsArrayJson(found->json)) {
          TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
        }

        size_t const n = TRI_LengthArrayJson(found->json);

        if (index < 0) {
          index = static_cast<int64_t>(n) + index;
        }

        if (index >= static_cast<int64_t>(n)) {
          result = v8::Undefined(isolate);
        } else {
          auto item = static_cast<TRI_json_t const*>(
              TRI_AtVector(&found->json->_value._objects, static_cast<size_t>(index)));
          result = TRI_ObjectJson(isolate, item);
        }
      }
    }

    TRI_V8_RETURN(result);
  }

  bool keySetAt(v8::Isolate* isolate, std::string const& key, int64_t index,
                v8::Handle<v8::Value> const& value) {
    WRITE_LOCKER(writeLocker, _lock);

    auto it = _hash.find(key);

    if (it == _hash.end()) {
      return false;
    } else {
      KeySpaceElement* found = (*it).second;
      if (!TRI_IsArrayJson(found->json)) {
        return false;
      }

      size_t const n = TRI_LengthArrayJson(found->json);

      if (index < 0) {
        return false;
      }

      auto json = TRI_ObjectToJson(isolate, value);
      if (json == nullptr) {
        return false;
      }

      if (index >= static_cast<int64_t>(n)) {
        // insert new element
        TRI_InsertVector(&found->json->_value._objects, json, static_cast<size_t>(index));
      } else {
        // overwrite existing element
        auto item = static_cast<TRI_json_t*>(
            TRI_AtVector(&found->json->_value._objects, static_cast<size_t>(index)));
        if (item != nullptr) {
          TRI_DestroyJson(item);
        }

        TRI_SetVector(&found->json->_value._objects, static_cast<size_t>(index), json);
      }

      // only free pointer to json, but not its internal structures
      TRI_Free(json);
    }

    return true;
  }

  char const* keyType(std::string const& key) {
    READ_LOCKER(readLocker, _lock);

    auto it = _hash.find(key);
    if (it != _hash.end()) {
      KeySpaceElement* found = (*it).second;
      TRI_json_t const* value = static_cast<KeySpaceElement*>(found)->json;

      switch (value->_type) {
        case TRI_JSON_NULL:
          return "null";
        case TRI_JSON_BOOLEAN:
          return "boolean";
        case TRI_JSON_NUMBER:
          return "number";
        case TRI_JSON_STRING:
        case TRI_JSON_STRING_REFERENCE:
          return "string";
        case TRI_JSON_ARRAY:
          return "list";
        case TRI_JSON_OBJECT:
          return "object";
        case TRI_JSON_UNUSED:
          break;
      }
    }

    return "undefined";
  }

  void keyMerge(v8::FunctionCallbackInfo<v8::Value> const& args, std::string const& key,
                v8::Handle<v8::Value> const& value, bool nullMeansRemove) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    if (!value->IsObject() || value->IsArray()) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    WRITE_LOCKER(writeLocker, _lock);

    auto [it, emplaced] = _hash.try_emplace(
      key,
      arangodb::lazyConstruct([&]{
        return new KeySpaceElement(key.c_str(), key.size(),
                                         TRI_ObjectToJson(isolate, value));
      })
    );


    if (emplaced) {
      TRI_V8_RETURN(value); // does a real return
    }

    KeySpaceElement* found = (*it).second;

    if (!TRI_IsObjectJson(found->json)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    TRI_json_t* other = TRI_ObjectToJson(isolate, value);

    if (other == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_json_t* merged = TRI_MergeJson(found->json, other, nullMeansRemove, false);
    TRI_FreeJson(other);

    if (merged == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    found->setValue(merged);

    TRI_V8_RETURN(TRI_ObjectJson(isolate, merged));
  }

 private:
  arangodb::basics::ReadWriteLock _lock;
  std::unordered_map<std::string, KeySpaceElement*> _hash;
};

struct UserStructures {
  struct {
    arangodb::basics::ReadWriteLock lock;
    std::unordered_map<std::string, KeySpace*> data;
  } hashes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

static inline TRI_vocbase_t& GetContextVocBase(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g->_vocbase != nullptr);

  return *static_cast<TRI_vocbase_t*>(v8g->_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash array by name
/// note that at least the read-lock must be held to use this function
////////////////////////////////////////////////////////////////////////////////

static KeySpace* GetKeySpace(TRI_vocbase_t* vocbase, std::string const& name) {
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  auto it = h->data.find(name);

  if (it != h->data.end()) {
    return (*it).second;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceCreate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "KEYSPACE_CREATE(<name>, <size>, <ignoreExisting>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  int64_t size = 0;

  if (args.Length() > 1) {
    size = TRI_ObjectToInt64(isolate, args[1]);
    if (size < 0 || size > static_cast<decltype(size)>(UINT32_MAX)) {
      TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <size>");
    }
  }

  bool ignoreExisting = false;

  if (args.Length() > 2) {
    ignoreExisting = TRI_ObjectToBoolean(isolate, args[2]);
  }

  auto ptr = std::make_unique<KeySpace>(static_cast<uint32_t>(size));
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  {
    WRITE_LOCKER(writeLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash != nullptr) {
      if (!ignoreExisting) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "hash already exists");
      }
      TRI_V8_RETURN_FALSE();
    }

    try {
      auto [it, emplaced] = h->data.try_emplace(name, ptr.get());
      if (emplaced) {
        ptr.release();
        TRI_ASSERT(it != h->data.end());
      }
    } catch (...) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceDrop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_DROP(<name>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  {
    WRITE_LOCKER(writeLocker, h->lock);
    auto it = h->data.find(name);

    if (it == h->data.end()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "Keyspace does not exist");
    }

    delete (*it).second;
    h->data.erase(it);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of items in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceCount(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_COUNT(<name>, <prefix>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  uint32_t count;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    if (args.Length() > 1) {
      std::string const prefix = TRI_ObjectToString(isolate, args[1]);
      count = hash->keyspaceCount(prefix);
    } else {
      count = hash->keyspaceCount();
    }
  }

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<int>(count)));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether a keyspace exists
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceExists(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_EXISTS(<name>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash != nullptr) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all keys of the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_KEYS(<name>, <prefix>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 1) {
    std::string const prefix = TRI_ObjectToString(isolate, args[1]);
    TRI_V8_RETURN(hash->keyspaceKeys(isolate, prefix));
  }

  TRI_V8_RETURN(hash->keyspaceKeys(isolate));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all data of the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceGet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_GET(<name>, <prefix>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 1) {
    std::string const prefix = TRI_ObjectToString(isolate, args[1]);
    TRI_V8_RETURN(hash->keyspaceGet(isolate, prefix));
  }

  TRI_V8_RETURN(hash->keyspaceGet(isolate));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all keys from the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceRemove(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_REMOVE(<name>, <prefix>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 1) {
    std::string const prefix = TRI_ObjectToString(isolate, args[1]);
    TRI_V8_RETURN(hash->keyspaceRemove(isolate, prefix));
  }

  TRI_V8_RETURN(hash->keyspaceRemove(isolate));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyGet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_GET(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  v8::Handle<v8::Value> result;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyGet(isolate, key);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeySet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_SET(<name>, <key>, <value>, <replace>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  bool replace = true;

  if (args.Length() > 3) {
    replace = TRI_ObjectToBoolean(isolate, args[3]);
  }

  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  bool result;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keySet(isolate, key, args[2], replace);
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calls global.KEY_SET('queue-control', 'databases-expire', 0);
////////////////////////////////////////////////////////////////////////////////
void TRI_ExpireFoxxQueueDatabaseCache(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase->isSystem());
  std::string const name = "queue-control";
  std::string const key = "databases-expire";

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  bool result;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);
    if (hash == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to find keyspace");
    }

    result = hash->keySet(key, 0);
  }
  if (!result) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to set key");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief conditionally set the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeySetCas(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "KEY_SET_CAS(<name>, <key>, <value>, <compare>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);

  if (args[2]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  int res;
  bool match = false;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    res = hash->keyCas(isolate, key, args[2], args[3], match);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (match) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyRemove(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_REMOVE(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  bool result;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyRemove(key);
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a key exists in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyExists(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_EXISTS(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  bool result;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyExists(key);
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase or decrease the value for a key in a keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyIncr(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_INCR(<name>, <key>, <value>)");
  }

  if (args.Length() >= 3 && !args[2]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_INCR(<name>, <key>, <value>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  double incr = 1.0;

  if (args.Length() >= 3) {
    incr = TRI_ObjectToDouble(isolate, args[2]);
  }

  double result;
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    int res = hash->keyIncr(key, incr, result);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN(v8::Number::New(isolate, result));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merges an object into the object with the specified key
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyUpdate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "KEY_UPDATE(<name>, <key>, <object>, <nullMeansRemove>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  bool nullMeansRemove = false;

  if (args.Length() > 3) {
    nullMeansRemove = TRI_ObjectToBoolean(isolate, args[3]);
  }

  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyMerge(args, key, args[2], nullMeansRemove);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all keys of the key
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_KEYS(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  TRI_V8_RETURN(hash->keyKeys(isolate, key));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all value of the hash array
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyValues(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_VALUES(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  TRI_V8_RETURN(hash->keyValues(isolate, key));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief right-pushes an element into a list value
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyPush(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_PUSH(<name>, <key>, <value>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  int res = hash->keyPush(isolate, key, args[2]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops an element from a list value
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyPop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_POP(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyPop(args, key);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer an element from a list value into another
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyTransfer(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_TRANSFER(<name>, <key-from>, <key-to>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const keyFrom = TRI_ObjectToString(isolate, args[1]);
  std::string const keyTo = TRI_ObjectToString(isolate, args[2]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyTransfer(args, keyFrom, keyTo);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an element at a specific list position
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyGetAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_GET_AT(<name>, <key>, <index>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  int64_t offset = TRI_ObjectToInt64(isolate, args[2]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyGetAt(args, key, offset);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set an element at a specific list position
////////////////////////////////////////////////////////////////////////////////

static void JS_KeySetAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_SET_AT(<name>, <key>, <index>, <value>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  int64_t offset = TRI_ObjectToInt64(isolate, args[2]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(&vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  int res = hash->keySetAt(isolate, key, offset, args[3]);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the type of the value for a key
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyType(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_TYPE(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  char const* result;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyType(key);
  }

  TRI_V8_RETURN_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of items in a compound value
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyCount(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_COUNT(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  auto h = &(static_cast<UserStructures*>(vocbase._userStructures)->hashes);
  uint32_t result;
  bool valid;

  {
    READ_LOCKER(readLocker, h->lock);
    auto hash = GetKeySpace(&vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    valid = hash->keyCount(key, result);
  }

  if (valid) {
    TRI_V8_RETURN(v8::Number::New(isolate, result));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the user structures for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateUserStructuresVocBase(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(vocbase->_userStructures == nullptr);

  vocbase->_userStructures = new UserStructures;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the user structures for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeUserStructuresVocBase(TRI_vocbase_t* vocbase) {
  if (vocbase->_userStructures != nullptr) {
    auto us = static_cast<UserStructures*>(vocbase->_userStructures);
    for (auto& hash : us->hashes.data) {
      if (hash.second != nullptr) {
        delete hash.second;
      }
    }
    delete us;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the user structures functions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8UserStructures(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // NOTE: the following functions are all experimental and might
  // change without further notice
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_CREATE"),
                               JS_KeyspaceCreate, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_DROP"),
                               JS_KeyspaceDrop, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_COUNT"),
                               JS_KeyspaceCount, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_EXISTS"),
                               JS_KeyspaceExists, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_KEYS"),
                               JS_KeyspaceKeys, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_REMOVE"),
                               JS_KeyspaceRemove, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_GET"),
                               JS_KeyspaceGet, true);

  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_SET"),
                               JS_KeySet, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_SET_CAS"),
                               JS_KeySetCas, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_GET"),
                               JS_KeyGet, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_REMOVE"),
                               JS_KeyRemove, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_EXISTS"),
                               JS_KeyExists, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_TYPE"),
                               JS_KeyType, true);

  // numeric functions
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_INCR"),
                               JS_KeyIncr, true);

  // list / array functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_UPDATE"),
                               JS_KeyUpdate, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_KEYS"),
                               JS_KeyKeys, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_VALUES"),
                               JS_KeyValues, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_COUNT"),
                               JS_KeyCount, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_PUSH"),
                               JS_KeyPush, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_POP"),
                               JS_KeyPop, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_TRANSFER"),
                               JS_KeyTransfer, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_GET_AT"),
                               JS_KeyGetAt, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEY_SET_AT"),
                               JS_KeySetAt, true);
}
