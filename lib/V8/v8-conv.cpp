////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "V8/v8-buffer.h"
#include "V8/v8-utils.h"

using namespace arangodb::basics;

/// @brief converts a V8 object to a string
std::string TRI_ObjectToString(v8::Isolate* isolate, v8::Handle<v8::Value> value) {
  if (value->IsObject() && V8Buffer::hasInstance(isolate, value)) {
    // argument is a buffer
    char const* data = V8Buffer::data(isolate, value.As<v8::Object>());
    size_t size = V8Buffer::length(isolate, value.As<v8::Object>());
    return std::string(data, size);
  }

  TRI_Utf8ValueNFC utf8Value(isolate, value);
  if (*utf8Value == nullptr) {
    return "";
  }

  return std::string(*utf8Value, utf8Value.length());
}

/// @brief converts an V8 object to an int64_t
int64_t TRI_ObjectToInt64(v8::Isolate* isolate, v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return static_cast<int64_t>(
        v8::Handle<v8::Number>::Cast(value)->NumberValue(TRI_IGETC).FromMaybe(0.0));
  }

  if (value->IsNumberObject()) {
    return static_cast<int64_t>(
        v8::Handle<v8::NumberObject>::Cast(value)->NumberValue(TRI_IGETC).FromMaybe(0.0));
  }

  return 0;
}

/// @brief converts an V8 object to a uint64_t
uint64_t TRI_ObjectToUInt64(v8::Isolate* isolate, v8::Handle<v8::Value> const value,
                            bool allowStringConversion) {
  if (value->IsNumber()) {
    return static_cast<uint64_t>(
        v8::Handle<v8::Number>::Cast(value)->NumberValue(TRI_IGETC).FromMaybe(0.0));
  }

  if (value->IsNumberObject()) {
    return static_cast<uint64_t>(
        v8::Handle<v8::NumberObject>::Cast(value)->NumberValue(TRI_IGETC).FromMaybe(0.0));
  }

  if (allowStringConversion && value->IsString()) {
    v8::String::Utf8Value str(isolate, value);
    return StringUtils::uint64(*str, str.length());
  }

  return 0;
}

/// @brief converts an V8 object to a double
double TRI_ObjectToDouble(v8::Isolate* isolate, v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return TRI_GET_DOUBLE(value);
  }

  if (value->IsNumberObject()) {
    return v8::Handle<v8::NumberObject>::Cast(value)->NumberValue(TRI_IGETC).FromMaybe(0.0);
  }
  return 0.0;
}

/// @brief converts an V8 object to a double with error handling
double TRI_ObjectToDouble(v8::Isolate* isolate,
                          v8::Handle<v8::Value> const value, bool& error) {
  error = false;

  if (value->IsNumber()) {
    return TRI_GET_DOUBLE(value);
  }

  if (value->IsNumberObject()) {
    return v8::Handle<v8::NumberObject>::Cast(value)->NumberValue(TRI_IGETC).FromMaybe(0.0);
  }

  error = true;

  return 0.0;
}

/// @brief converts an V8 object to a boolean
bool TRI_ObjectToBoolean(v8::Isolate* isolate, v8::Handle<v8::Value> const value) {
  if (value->IsBoolean()) {
    return value->IsTrue();
  }

  if (value->IsBooleanObject()) {
    return v8::Local<v8::BooleanObject>::Cast(value)->ValueOf();
  }

  return false;
}

/// @brief extracts an optional boolean property from a V8 object
bool TRI_GetOptionalBooleanProperty(v8::Isolate* isolate, v8::Handle<v8::Object> const obj,
                                    const char* property, bool defaultValue) {
  auto value = obj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, property));
  if (!value.IsEmpty()) {
    return TRI_ObjectToBoolean(isolate, value.FromMaybe(v8::Local<v8::Value>()));
  } else {
    return defaultValue;
  }
}
