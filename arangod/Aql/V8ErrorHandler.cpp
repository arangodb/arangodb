////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "V8ErrorHandler.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

namespace arangodb::aql {

/// @brief checks if a V8 exception has occurred and throws an appropriate C++
/// exception from it if so
void handleV8Error(v8::TryCatch& tryCatch, v8::Handle<v8::Value>& result) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  bool failed = false;

  if (tryCatch.HasCaught()) {
    // caught a V8 exception
    if (!tryCatch.CanContinue()) {
      // request was canceled
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
          isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    // request was not canceled, but some other error occurred
    // peek into the exception
    if (tryCatch.Exception()->IsObject()) {
      // cast the exception to an object

      v8::Handle<v8::Array> objValue =
          v8::Handle<v8::Array>::Cast(tryCatch.Exception());
      v8::Handle<v8::String> errorNum =
          TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::ErrorNum);
      v8::Handle<v8::String> errorMessage =
          TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::ErrorMessage);

      TRI_Utf8ValueNFC stacktrace(
          isolate,
          tryCatch.StackTrace(context).FromMaybe(v8::Local<v8::Value>()));

      if (TRI_HasProperty(context, isolate, objValue, errorNum) &&
          TRI_HasProperty(context, isolate, objValue, errorMessage)) {
        v8::Handle<v8::Value> errorNumValue =
            objValue->Get(context, errorNum).FromMaybe(v8::Local<v8::Value>());
        v8::Handle<v8::Value> errorMessageValue =
            objValue->Get(context, errorMessage)
                .FromMaybe(v8::Local<v8::Value>());

        // found something that looks like an ArangoError
        if ((errorNumValue->IsNumber() || errorNumValue->IsNumberObject()) &&
            (errorMessageValue->IsString() ||
             errorMessageValue->IsStringObject())) {
          auto errorCode = ErrorCode{
              static_cast<int>(TRI_ObjectToInt64(isolate, errorNumValue))};
          std::string errorMessage(
              TRI_ObjectToString(isolate, errorMessageValue));

          if (*stacktrace && stacktrace.length() > 0) {
            errorMessage += "\nstacktrace of offending AQL function: ";
            errorMessage += *stacktrace;
          }

          THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, errorMessage);
        }
      }

      // exception is no ArangoError
      std::string details(TRI_ObjectToString(isolate, tryCatch.Exception()));

      if (*stacktrace && stacktrace.length() > 0) {
        details += "\nstacktrace of offending AQL function: ";
        details += *stacktrace;
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_SCRIPT, details);
    }

    failed = true;
  }

  if (result.IsEmpty()) {
    failed = true;
  }

  if (failed) {
    // we can't figure out what kind of error occurred and throw a generic error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_SCRIPT,
                                   "unknown error in scripting");
  }

  // if we get here, no exception has been raised
}

}  // namespace arangodb::aql
