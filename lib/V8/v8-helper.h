////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <optional>

namespace arangodb {

inline std::string stringify(v8::Isolate* isolate, v8::Handle<v8::Value> value) {
  auto context = TRI_IGETC;
  // function converts js object to string using JSON.stringify
  if (value.IsEmpty()) {
    return std::string{};
  }
  auto ctx = isolate->GetCurrentContext();
  v8::Local<v8::Object> json = ctx->Global()
    ->Get(context,
          TRI_V8_ASCII_STRING(isolate, "JSON"))
    .FromMaybe(v8::Local<v8::Value>())
    ->ToObject(ctx)
    .FromMaybe(v8::Local<v8::Object>());
  v8::Local<v8::Function> stringify =
    json->Get(context, TRI_V8_ASCII_STRING(isolate, "stringify")).FromMaybe(v8::Local<v8::Value>()).As<v8::Function>();
  v8::Local<v8::Value> args[1] = {value};
  v8::Local<v8::Value> jsString = stringify->Call(TRI_IGETC, json, 1, args).FromMaybe(v8::Local<v8::Value>());
  v8::String::Utf8Value const rv(isolate, jsString);
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
                                                         ErrorCode errorCode) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
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
    std::string errorMessage =
        *v8::String::Utf8Value(isolate, exception->ToString(TRI_IGETC).FromMaybe(
                                            v8::Local<v8::String>()));
    std::get<1>(rv) = true;
    std::get<2>(rv).reset(errorCode, errorMessage);
    tryCatch.Reset();
    return rv;
  }

  if (!exception->IsObject()) {
    // we have no idea what this error is about
    std::get<1>(rv) = true;
    TRI_Utf8ValueNFC exception(isolate, tryCatch.Exception());
    char const* exceptionString = *exception;
    if (exceptionString == nullptr) {
      std::get<2>(rv).reset(errorCode, "JavaScript exception");
    } else {
      std::get<2>(rv).reset(errorCode, exceptionString);
    }
    return rv;
  }

  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(exception);

  auto errorNum = std::optional<ErrorCode>{};

  if (TRI_HasProperty(context, isolate, object, "errorNum")) {
    errorNum = ErrorCode{static_cast<int>(TRI_ObjectToInt64(
        isolate, object->Get(context, TRI_V8_ASCII_STRING(isolate, "errorNum"))
                     .FromMaybe(v8::Local<v8::Value>())))};
  }

  try {
    if (errorNum.has_value() &&
        (TRI_HasProperty(context, isolate, object, "errorMessage") ||
         TRI_HasProperty(context, isolate, object, "message"))) {
      std::string errorMessage;
      if (TRI_HasProperty(context, isolate, object, "errorMessage")) {
        v8::String::Utf8Value msg(isolate,
                                  object->Get(context,
                                              TRI_V8_ASCII_STRING(isolate, "errorMessage"))
                                  .FromMaybe(v8::Local<v8::Value>()));
        if (*msg != nullptr) {
          errorMessage = std::string(*msg, msg.length());
        }
      } else {
        v8::String::Utf8Value msg(isolate,
                                  object->Get(context,
                                              TRI_V8_ASCII_STRING(isolate, "message"))
                                  .FromMaybe(v8::Local<v8::Value>()));
        if (*msg != nullptr) {
          errorMessage = std::string(*msg, msg.length());
        }
      }
      std::get<1>(rv) = true;
      std::get<2>(rv).reset(*errorNum, errorMessage);
      tryCatch.Reset();
      return rv;
    }

    if (TRI_HasProperty(context, isolate, object, "name") &&
        TRI_HasProperty(context, isolate, object, "message")) {
      std::string name;
      v8::String::Utf8Value nameString(isolate,
                                       object->Get(context,
                                                   TRI_V8_ASCII_STRING(isolate, "name"))
                                       .FromMaybe(v8::Local<v8::Value>()));
      if (*nameString != nullptr) {
        name = std::string(*nameString, nameString.length());
      }

      std::string message;
      v8::String::Utf8Value messageString(isolate,
                                          object->Get(context,
                                                      TRI_V8_ASCII_STRING(isolate, "message"))
                                          .FromMaybe(v8::Local<v8::Value>()));
      if (*messageString != nullptr) {
        message = std::string(*messageString, messageString.length());
      }
      if (name == "TypeError") {
        std::get<2>(rv).reset(TRI_ERROR_TYPE_ERROR, message);
      } else {
        if (!errorNum.has_value()) {
          errorNum = errorCode;
        }
        std::get<2>(rv).reset(*errorNum, name + ": " + message);
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
