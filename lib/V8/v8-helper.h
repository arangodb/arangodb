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
#include "Basics/Common.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "v8-globals.h"

namespace arangodb {

inline std::string stringify(v8::Isolate* isolate, v8::Handle<v8::Value> value) {
  // function converts js object to string using JSON.stringify
  if (value.IsEmpty()) {
    return std::string{};
  }
  v8::Local<v8::Object> json = isolate->GetCurrentContext()
                                   ->Global()
                                   ->Get(TRI_V8_ASCII_STRING(isolate, "JSON"))
                                   ->ToObject();
  v8::Local<v8::Function> stringify =
      json->Get(TRI_V8_ASCII_STRING(isolate, "stringify")).As<v8::Function>();
  v8::Local<v8::Value> args[1] = {value};
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
  v8gHelper(v8::Isolate* isolate, v8::TryCatch& tryCatch,
            v8::Handle<v8::Value>& request, v8::Handle<v8::Value>& response)
      : _isolate(isolate), _tryCatch(tryCatch) {
    TRI_GET_GLOBALS();
    _v8g = v8g;
    _v8g->_currentRequest = request;
  }

  void cancel(bool doCancel) {
    if (doCancel) {
      _v8g->_canceled = true;
    }
  }

  ~v8gHelper() {
    if (_v8g->_canceled) {
      return;
    }

    if (_tryCatch.HasCaught() && !_tryCatch.CanContinue()) {
      _v8g->_canceled = true;
    } else {
      _v8g->_currentRequest = v8::Undefined(_isolate);
      _v8g->_currentResponse = v8::Undefined(_isolate);
    }
  }
};

inline bool isContextCanceled(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();
  return v8g->_canceled;
}

inline std::tuple<bool, bool, Result> extractArangoError(v8::Isolate* isolate,
                                                         v8::TryCatch& tryCatch,
                                                         int errorCode) {
  // function tries to receive arango error form tryCatch Object
  // return tuple:
  //   bool - can continue
  //   bool - could convert
  //   result - extracted arango error
  std::tuple<bool, bool, Result> rv = {};

  std::get<0>(rv) = true;
  std::get<1>(rv) = false;

  if (!tryCatch.CanContinue()) {
    std::get<0>(rv) = false;
    std::get<1>(rv) = true;
    std::get<2>(rv).reset(TRI_ERROR_REQUEST_CANCELED);
    TRI_GET_GLOBALS();
    v8g->_canceled = true;
    return rv;
  }

  v8::Handle<v8::Value> exception = tryCatch.Exception();
  if (exception->IsString()) {
    // the error is a plain string
    std::string errorMessage = *v8::String::Utf8Value(exception->ToString());
    std::get<1>(rv) = true;
    std::get<2>(rv).reset(errorCode, errorMessage);
    tryCatch.Reset();
    return rv;
  }

  if (!exception->IsObject()) {
    // we have no idea what this error is about
    std::get<1>(rv) = true;
    TRI_Utf8ValueNFC exception(tryCatch.Exception());
    char const* exceptionString = *exception;
    if (exceptionString == nullptr) {
      std::get<2>(rv).reset(errorCode, "JavaScript exception");
    } else {
      std::get<2>(rv).reset(errorCode, exceptionString);
    }
    return rv;
  }

  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(exception);

  int errorNum = -1;

  if (object->Has(TRI_V8_ASCII_STRING(isolate, "errorNum"))) {
    errorNum = static_cast<int>(TRI_ObjectToInt64(
        object->Get(TRI_V8_ASCII_STRING(isolate, "errorNum"))));
  }

  try {
    if ((errorNum != -1) &&
        (object->Has(TRI_V8_ASCII_STRING(isolate, "errorMessage")) ||
         object->Has(TRI_V8_ASCII_STRING(isolate, "message")))) {
      std::string errorMessage;
      if (object->Has(TRI_V8_ASCII_STRING(isolate, "errorMessage"))) {
        v8::String::Utf8Value msg(
            object->Get(TRI_V8_ASCII_STRING(isolate, "errorMessage")));
        if (*msg != nullptr) {
          errorMessage = std::string(*msg, msg.length());
        }
      } else {
        v8::String::Utf8Value msg(
            object->Get(TRI_V8_ASCII_STRING(isolate, "message")));
        if (*msg != nullptr) {
          errorMessage = std::string(*msg, msg.length());
        }
      }
      std::get<1>(rv) = true;
      std::get<2>(rv).reset(errorNum, errorMessage);
      tryCatch.Reset();
      return rv;
    }

    if (object->Has(TRI_V8_ASCII_STRING(isolate, "name")) &&
        object->Has(TRI_V8_ASCII_STRING(isolate, "message"))) {
      std::string name;
      v8::String::Utf8Value nameString(
          object->Get(TRI_V8_ASCII_STRING(isolate, "name")));
      if (*nameString != nullptr) {
        name = std::string(*nameString, nameString.length());
      }

      std::string message;
      v8::String::Utf8Value messageString(
          object->Get(TRI_V8_ASCII_STRING(isolate, "message")));
      if (*messageString != nullptr) {
        message = std::string(*messageString, messageString.length());
      }
      if (name == "TypeError") {
        std::get<2>(rv).reset(TRI_ERROR_TYPE_ERROR, message);
      } else {
        if (errorNum == -1) {
          errorNum = errorCode;
        }
        std::get<2>(rv).reset(errorNum, name + ": " + message);
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
}  // namespace arangodb
#endif
