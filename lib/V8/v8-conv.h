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

#ifndef ARANGODB_V8_V8__CONV_H
#define ARANGODB_V8_V8__CONV_H 1

#include "Basics/Common.h"
#include "V8/v8-globals.h"

/// @brief converts an V8 object to a string
std::string TRI_ObjectToString(v8::Handle<v8::Value>);
std::string TRI_ObjectToString(v8::Isolate* isolate, v8::Handle<v8::Value>);

/// @brief converts an V8 object to an int64_t
int64_t TRI_ObjectToInt64(v8::Handle<v8::Value> const);

/// @brief converts an V8 object to a uint64_t
uint64_t TRI_ObjectToUInt64(v8::Handle<v8::Value> const, bool);

/// @brief converts a V8 object to a double
double TRI_ObjectToDouble(v8::Handle<v8::Value> const);

/// @brief converts a V8 object to a double with error handling
double TRI_ObjectToDouble(v8::Handle<v8::Value> const, bool& error);

/// @brief converts a V8 object to a boolean
bool TRI_ObjectToBoolean(v8::Handle<v8::Value> const);

#endif
