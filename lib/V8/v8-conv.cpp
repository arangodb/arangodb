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

#include "v8-conv.h"

#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "V8/v8-utils.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NULL into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonNull(v8::Isolate* isolate,
                                                   TRI_json_t const* json) {
  return v8::Null(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t BOOLEAN into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonBoolean(v8::Isolate* isolate,
                                                      TRI_json_t const* json) {
  return v8::Boolean::New(isolate, json->_value._boolean);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t NUMBER into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonNumber(v8::Isolate* isolate,
                                                     TRI_json_t const* json) {
  return v8::Number::New(isolate, json->_value._number);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t STRING into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectJsonString(v8::Isolate* isolate,
                                                     TRI_json_t const* json) {
  return TRI_V8_PAIR_STRING(json->_value._string.data,
                            json->_value._string.length - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t OBJECT into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectJsonObject(v8::Isolate* isolate,
                                              TRI_json_t const* json) {
  v8::Handle<v8::Object> object = v8::Object::New(isolate);

  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  size_t const n = TRI_LengthVector(&json->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    TRI_json_t const* key = static_cast<TRI_json_t const*>(
        TRI_AddressVector(&json->_value._objects, i));

    if (!TRI_IsStringJson(key)) {
      continue;
    }

    TRI_json_t const* element = static_cast<TRI_json_t const*>(
        TRI_AddressVector(&json->_value._objects, i + 1));

    auto val = TRI_ObjectJson(isolate, element);
    if (!val.IsEmpty()) {
      auto k = TRI_V8_PAIR_STRING(key->_value._string.data,
                                  key->_value._string.length - 1);
      if (!k.IsEmpty()) {
        object->ForceSet(TRI_V8_PAIR_STRING(key->_value._string.data,
                                            key->_value._string.length - 1),
                         val);
      }
    }
  }

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t ARRAY into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectJsonArray(v8::Isolate* isolate,
                                             TRI_json_t const* json) {
  uint32_t const n = static_cast<uint32_t>(TRI_LengthArrayJson(json));

  v8::Handle<v8::Array> object = v8::Array::New(isolate, static_cast<int>(n));

  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  uint32_t j = 0;
  for (uint32_t i = 0; i < n; ++i) {
    TRI_json_t const* element = static_cast<TRI_json_t const*>(
        TRI_AddressVector(&json->_value._objects, i));
    v8::Handle<v8::Value> val = TRI_ObjectJson(isolate, element);

    if (!val.IsEmpty()) {
      object->Set(j++, val);
    }
  }

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts keys or values from a TRI_json_t* object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExtractObject(v8::Isolate* isolate,
                                           TRI_json_t const* json,
                                           size_t offset) {
  v8::EscapableHandleScope scope(isolate);

  if (json == nullptr || json->_type != TRI_JSON_OBJECT) {
    return scope.Escape<v8::Value>(v8::Undefined(isolate));
  }

  size_t const n = TRI_LengthVector(&json->_value._objects);

  v8::Handle<v8::Array> result =
      v8::Array::New(isolate, static_cast<int>(n / 2));
  uint32_t count = 0;

  for (size_t i = offset; i < n; i += 2) {
    TRI_json_t const* value =
        static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));

    if (value != nullptr) {
      result->Set(count++, TRI_ObjectJson(isolate, value));
    }
  }

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the keys of a TRI_json_t* object into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_KeysJson(v8::Isolate* isolate,
                                   TRI_json_t const* json) {
  return ExtractObject(isolate, json, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the values of a TRI_json_t* object into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ValuesJson(v8::Isolate* isolate,
                                     TRI_json_t const* json) {
  return ExtractObject(isolate, json, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ObjectJson(v8::Isolate* isolate,
                                     TRI_json_t const* json) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a TRI_json_t value
////////////////////////////////////////////////////////////////////////////////

static int ObjectToJson(v8::Isolate* isolate, TRI_json_t* result,
                        v8::Handle<v8::Value> const parameter,
                        std::set<int>& seenHashes,
                        std::vector<v8::Handle<v8::Object>>& seenObjects) {
  v8::HandleScope scope(isolate);

  if (parameter->IsNull()) {
    TRI_InitNullJson(result);
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsBoolean()) {
    v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean();
    TRI_InitBooleanJson(result, booleanParameter->Value());
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsNumber()) {
    v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
    TRI_InitNumberJson(result, numberParameter->Value());
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsString()) {
    v8::Handle<v8::String> stringParameter = parameter->ToString();
    TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, stringParameter);

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
    TRI_InitArrayJson(TRI_UNKNOWN_MEM_ZONE, result, static_cast<size_t>(n));
    int res =
        TRI_ReserveVector(&result->_value._objects, static_cast<size_t>(n));

    if (res != TRI_ERROR_NO_ERROR) {
      // result array could not be allocated
      TRI_InitNullJson(result);
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < n; ++i) {
      // get address of next element
      auto next =
          static_cast<TRI_json_t*>(TRI_NextVector(&result->_value._objects));
      // the reserve call above made sure we could not have run out of memory
      TRI_ASSERT(next != nullptr);

      res = ObjectToJson(isolate, next, array->Get(i), seenHashes, seenObjects);

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
                                      ->BooleanValue());
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsNumberObject()) {
      TRI_InitNumberJson(
          result, v8::Handle<v8::NumberObject>::Cast(parameter)->NumberValue());
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsStringObject()) {
      v8::Handle<v8::String> stringParameter(parameter->ToString());
      TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, stringParameter);

      if (*str == nullptr) {
        TRI_InitNullJson(result);
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      // this passes ownership for the utf8 string to the JSON object
      TRI_InitStringJson(result, str.steal(), str.length());
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsRegExp() || parameter->IsFunction() ||
        parameter->IsExternal()) {
      TRI_InitNullJson(result);
      return TRI_ERROR_BAD_PARAMETER;
    }

    v8::Handle<v8::Object> o = parameter->ToObject();

    // first check if the object has a "toJSON" function
    v8::Handle<v8::String> toJsonString = TRI_V8_PAIR_STRING("toJSON", 6);
    if (o->Has(toJsonString)) {
      // call it if yes
      v8::Handle<v8::Value> func = o->Get(toJsonString);
      if (func->IsFunction()) {
        v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(func);

        v8::Handle<v8::Value> args;
        v8::Handle<v8::Value> converted = toJson->Call(o, 0, &args);

        if (!converted.IsEmpty()) {
          // return whatever toJSON returned
          TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, converted->ToString());

          if (*str == nullptr) {
            TRI_InitNullJson(result);
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          // this passes ownership for the utf8 string to the JSON object
          TRI_InitStringJson(result, str.steal(), str.length());
          return TRI_ERROR_NO_ERROR;
        }
      }

      // fall-through intentional
    }

    int hash = o->GetIdentityHash();

    if (seenHashes.find(hash) != seenHashes.end()) {
      // LOG(TRACE) << "found hash " << hash;

      for (auto it : seenObjects) {
        if (parameter->StrictEquals(it)) {
          // object is recursive
          TRI_InitNullJson(result);
          return TRI_ERROR_BAD_PARAMETER;
        }
      }
    } else {
      seenHashes.emplace(hash);
    }

    seenObjects.emplace_back(o);

    v8::Handle<v8::Array> names = o->GetOwnPropertyNames();
    uint32_t const n = names->Length();

    // allocate the result object buffer in one go
    TRI_InitObjectJson(TRI_UNKNOWN_MEM_ZONE, result, static_cast<size_t>(n));
    int res = TRI_ReserveVector(&result->_value._objects,
                                static_cast<size_t>(n * 2));  // key + value

    if (res != TRI_ERROR_NO_ERROR) {
      // result object buffer could not be allocated
      TRI_InitNullJson(result);
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < n; ++i) {
      // process attribute name
      v8::Handle<v8::Value> key = names->Get(i);
      TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, key);

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

      res = ObjectToJson(isolate, next, o->Get(key), seenHashes, seenObjects);

      if (res != TRI_ERROR_NO_ERROR) {
        // to mimic behavior of previous ArangoDB versions, we need to silently
        // ignore this error
        // now free the attributeName string and return the elements to the
        // vector
        TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, attributeName);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a json_t value
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ObjectToJson(v8::Isolate* isolate,
                             v8::Handle<v8::Value> const parameter) {
  TRI_json_t* json = TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == nullptr) {
    return nullptr;
  }

  std::set<int> seenHashes;
  std::vector<v8::Handle<v8::Object>> seenObjects;
  int res = ObjectToJson(isolate, json, parameter, seenHashes, seenObjects);

  if (res != TRI_ERROR_NO_ERROR) {
    // some processing error occurred
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return nullptr;
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a string
////////////////////////////////////////////////////////////////////////////////

std::string TRI_ObjectToString(v8::Handle<v8::Value> const value) {
  TRI_Utf8ValueNFC utf8Value(TRI_UNKNOWN_MEM_ZONE, value);

  if (*utf8Value == nullptr) {
    return "";
  }

  return std::string(*utf8Value, utf8Value.length());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to an int64_t
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_ObjectToInt64(v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return static_cast<int64_t>(value->ToNumber()->Value());
  }

  if (value->IsNumberObject()) {
    return static_cast<int64_t>(v8::Handle<v8::NumberObject>::Cast(value)->NumberValue());
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a uint64_t
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_ObjectToUInt64(v8::Handle<v8::Value> const value,
                            bool allowStringConversion) {
  if (value->IsNumber()) {
    return static_cast<uint64_t>(value->ToNumber()->Value());
  }

  if (value->IsNumberObject()) {
    return static_cast<uint64_t>(v8::Handle<v8::NumberObject>::Cast(value)->NumberValue());
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

double TRI_ObjectToDouble(v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return value->ToNumber()->Value();
  }

  if (value->IsNumberObject()) {
    return v8::Handle<v8::NumberObject>::Cast(value)->NumberValue();
  }

  return 0.0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a double with error handling
////////////////////////////////////////////////////////////////////////////////

double TRI_ObjectToDouble(v8::Handle<v8::Value> const value, bool& error) {
  error = false;

  if (value->IsNumber()) {
    return value->ToNumber()->Value();
  }

  if (value->IsNumberObject()) {
    return v8::Handle<v8::NumberObject>::Cast(value)->NumberValue();
  }

  error = true;

  return 0.0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a boolean
////////////////////////////////////////////////////////////////////////////////

bool TRI_ObjectToBoolean(v8::Handle<v8::Value> const value) {
  if (value->IsBoolean()) {
    return value->ToBoolean()->Value();
  } 

  if (value->IsBooleanObject()) {
    return v8::Handle<v8::BooleanObject>::Cast(value)->BooleanValue();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the V8 conversion module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Conversions(v8::Handle<v8::Context> context) {
  // nothing special to do here
}
