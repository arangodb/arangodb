////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8_V8__HELPER_H
#define ARANGODB_V8_V8__HELPER_H 1

#include <v8.h>
#include "v8-globals.h"
namespace arangodb {

inline std::string stringify(v8::Isolate* isolate, v8::Handle<v8::Value> value) {
  // function converts js object to string using JSON.stringify
	if (value.IsEmpty()) {
   return std::string{};
  }
  v8::Local<v8::Object> json = isolate->GetCurrentContext()->Global()->Get(TRI_V8_ASCII_STRING("JSON"))->ToObject();
  v8::Local<v8::Function> stringify = json->Get(TRI_V8_ASCII_STRING("stringify")).As<v8::Function>();
  v8::Local<v8::Value> jsString = stringify->Call(json, 1, &value);
  v8::String::Utf8Value const rv(jsString);
  return std::string(*rv, rv.length());
}

}
#endif
