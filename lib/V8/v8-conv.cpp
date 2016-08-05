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
//#include "Basics/tri-strings.h"
#include "V8/v8-buffer.h"
#include "V8/v8-utils.h"

using namespace arangodb::basics;

/// @brief converts a V8 object to a string
std::string TRI_ObjectToString(v8::Handle<v8::Value> value) {
  TRI_Utf8ValueNFC utf8Value(TRI_UNKNOWN_MEM_ZONE, value);

  if (*utf8Value == nullptr) {
    return "";
  }

  return std::string(*utf8Value, utf8Value.length());
}

/// @brief converts a V8 object to a string
std::string TRI_ObjectToString(v8::Isolate* isolate, v8::Handle<v8::Value> value) {
  if (value->IsObject() && V8Buffer::hasInstance(isolate, value)) {
    // argument is a buffer
    char const* data = V8Buffer::data(value.As<v8::Object>());
    size_t size = V8Buffer::length(value.As<v8::Object>());
    return std::string(data, size);
  }

  return TRI_ObjectToString(value);
}

/// @brief converts an V8 object to an int64_t
int64_t TRI_ObjectToInt64(v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return static_cast<int64_t>(value->ToNumber()->Value());
  }

  if (value->IsNumberObject()) {
    return static_cast<int64_t>(v8::Handle<v8::NumberObject>::Cast(value)->NumberValue());
  }

  return 0;
}

/// @brief converts an V8 object to a uint64_t
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

/// @brief converts an V8 object to a double
double TRI_ObjectToDouble(v8::Handle<v8::Value> const value) {
  if (value->IsNumber()) {
    return value->ToNumber()->Value();
  }

  if (value->IsNumberObject()) {
    return v8::Handle<v8::NumberObject>::Cast(value)->NumberValue();
  }

  return 0.0;
}

/// @brief converts an V8 object to a double with error handling
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

/// @brief converts an V8 object to a boolean
bool TRI_ObjectToBoolean(v8::Handle<v8::Value> const value) {
  if (value->IsBoolean()) {
    return value->ToBoolean()->Value();
  } 

  if (value->IsBooleanObject()) {
    return v8::Handle<v8::BooleanObject>::Cast(value)->BooleanValue();
  }

  return false;
}

