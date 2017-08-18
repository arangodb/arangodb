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
#include "V8/v8-conv.h"
namespace arangodb {

inline std::string stringify(v8::Isolate* isolate, v8::Handle<v8::Value> value) {
  // function converts js object to string using JSON.stringify
	if (value.IsEmpty()) {
   return std::string{};
  }
  v8::Local<v8::Object> json = isolate->GetCurrentContext()->Global()->Get(TRI_V8_ASCII_STRING("JSON"))->ToObject();
  v8::Local<v8::Function> stringify = json->Get(TRI_V8_ASCII_STRING("stringify")).As<v8::Function>();
  v8::Local<v8::Value> args[1] = { value };
  v8::Local<v8::Value> jsString = stringify->Call(json, 1, args);
  v8::String::Utf8Value const rv(jsString);
  return std::string(*rv, rv.length());
}

class v8gHelper {
  // raii helper
  TRI_v8_global_t* _v8g;
  v8::Isolate* _isolate;
  v8::TryCatch& _tryCatch;

public:
  v8gHelper(v8::Isolate* isolate
           ,v8::TryCatch& tryCatch
           ,v8::Handle<v8::Value>& request
           ,v8::Handle<v8::Value>& response
           )
           : _isolate(isolate)
           , _tryCatch(tryCatch)
  {
    TRI_GET_GLOBALS();
    _v8g = v8g;
    _v8g->_currentRequest = request;
  }

  void cancel(bool doCancel){
    if(doCancel){
      _v8g->_canceled=true;
    }
  }

  ~v8gHelper() {
    if(_v8g->_canceled){
      return;
    }

    if(_tryCatch.HasCaught() && !_tryCatch.CanContinue()){
      _v8g->_canceled=true;
    } else {
      _v8g->_currentRequest = v8::Undefined(_isolate);
      _v8g->_currentResponse = v8::Undefined(_isolate);
    }
  }
};

inline bool isContextCanceled(v8::Isolate* isolate){
  TRI_GET_GLOBALS();
  return v8g->_canceled;
}

inline std::tuple<bool,bool,Result> extractArangoError(v8::Isolate* isolate, v8::TryCatch& tryCatch){
  // function tries to receive arango error form tryCatch Object
  // return tuple:
	//   bool - can continue
  //   bool - could convert
  //   result - extracted arango error
  std::tuple<bool,bool,Result> rv = {};

  std::get<0>(rv) = true;
  std::get<1>(rv) = false;

  if(!tryCatch.CanContinue()){
    std::get<0>(rv) = false;
    TRI_GET_GLOBALS();
    v8g->_canceled = true;
  }

  v8::Handle<v8::Value> exception = tryCatch.Exception();
  if(!exception->IsObject()){
    return rv;
  }

  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(exception);

  try {

    if(object->Has(TRI_V8_ASCII_STRING("errorNum")) &&
       object->Has(TRI_V8_ASCII_STRING("errorMessage"))
      )
    {
      int errorNum = static_cast<int>(TRI_ObjectToInt64(object->Get(TRI_V8_ASCII_STRING("errorNum"))));
      std::string  errorMessage = *v8::String::Utf8Value(object->Get(TRI_V8_ASCII_STRING("errorMessage")));
      std::get<1>(rv) = true;
      std::get<2>(rv).reset(errorNum,errorMessage);
      tryCatch.Reset();
      return rv;
    }

    if(object->Has(TRI_V8_ASCII_STRING("name")) &&
       object->Has(TRI_V8_ASCII_STRING("message"))
      )
    {
      std::string  name = *v8::String::Utf8Value(object->Get(TRI_V8_ASCII_STRING("name")));
      std::string  message = *v8::String::Utf8Value(object->Get(TRI_V8_ASCII_STRING("message")));
      if(name == "TypeError"){
        std::get<2>(rv).reset(TRI_ERROR_TYPE_ERROR, message);
      } else {
        std::get<2>(rv).reset(TRI_ERROR_INTERNAL, name + ": " + message);
      }
      std::get<1>(rv) = true;
      tryCatch.Reset();
      return rv;
    }
  } catch (...) {
    // fail to extract but do nothing about it
  }

  return rv;
}
}
#endif
