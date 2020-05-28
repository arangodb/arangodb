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

#include "V8Executor.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/Functions.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

using namespace arangodb::aql;

/// @brief checks if a V8 exception has occurred and throws an appropriate C++
/// exception from it if so
void V8Executor::HandleV8Error(v8::TryCatch& tryCatch, v8::Handle<v8::Value>& result,
                               arangodb::basics::StringBuffer* const buffer,
                               bool duringCompile) {
  ISOLATE;
  auto context = TRI_IGETC;
  bool failed = false;

  if (tryCatch.HasCaught()) {
    // caught a V8 exception
    if (!tryCatch.CanContinue()) {
      // request was canceled
      TRI_GET_GLOBALS();
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    // request was not canceled, but some other error occurred
    // peek into the exception
    if (tryCatch.Exception()->IsObject()) {
      // cast the exception to an object

      v8::Handle<v8::Array> objValue = v8::Handle<v8::Array>::Cast(tryCatch.Exception());
      v8::Handle<v8::String> errorNum =
          TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::ErrorNum);
      v8::Handle<v8::String> errorMessage =
          TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::ErrorMessage);

      TRI_Utf8ValueNFC stacktrace(isolate, tryCatch.StackTrace(context).FromMaybe(v8::Local<v8::Value>()));

      if (TRI_HasProperty(context, isolate, objValue, errorNum) &&
          TRI_HasProperty(context, isolate, objValue, errorMessage)) {
        v8::Handle<v8::Value> errorNumValue = objValue->Get(context, errorNum).FromMaybe(v8::Local<v8::Value>());
        v8::Handle<v8::Value> errorMessageValue = objValue->Get(context, errorMessage).FromMaybe(v8::Local<v8::Value>());

        // found something that looks like an ArangoError
        if ((errorNumValue->IsNumber() || errorNumValue->IsNumberObject()) &&
            (errorMessageValue->IsString() || errorMessageValue->IsStringObject())) {
          int errorCode = static_cast<int>(TRI_ObjectToInt64(isolate, errorNumValue));
          std::string errorMessage(TRI_ObjectToString(isolate, errorMessageValue));

          if (*stacktrace && stacktrace.length() > 0) {
            errorMessage += "\nstacktrace of offending AQL function: ";
            errorMessage += *stacktrace;
          }

          THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, errorMessage);
        }
      }

      // exception is no ArangoError
      std::string details(TRI_ObjectToString(isolate, tryCatch.Exception()));

      if (buffer) {
        // std::string script(buffer->c_str(), buffer->length());
        LOG_TOPIC("98afd", ERR, arangodb::Logger::FIXME)
            << details << " " << Logger::CHARS(buffer->c_str(), buffer->length());
        details += "\nSee log for more details";
      }
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
    std::string msg("unknown error in scripting");
    if (duringCompile) {
      msg += " (during compilation)";
    }
    if (buffer) {
      // std::string script(buffer->c_str(), buffer->length());
      LOG_TOPIC("477ee", ERR, arangodb::Logger::FIXME)
          << msg << " " << Logger::CHARS(buffer->c_str(), buffer->length());
      msg += " See log for details";
    }
    // we can't figure out what kind of error occurred and throw a generic error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_SCRIPT, msg);
  }

  // if we get here, no exception has been raised
}
