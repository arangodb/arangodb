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

#include "JSLoader.h"

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

JSLoader::JSLoader() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a named script in the global context
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> JSLoader::executeGlobalScript(v8::Isolate* isolate,
                                                    v8::Handle<v8::Context> context,
                                                    std::string const& name) {
  v8::EscapableHandleScope scope(isolate);
  v8::TryCatch tryCatch;
  v8::Handle<v8::Value> result;

  findScript(name);

  std::map<std::string, std::string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    // correct the path/name
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "unknown script '" << StringUtils::correctPath(name) << "'";
    return v8::Undefined(isolate);
  }

  result = TRI_ExecuteJavaScriptString(isolate, context,
                                       TRI_V8_STD_STRING(isolate, i->second),
                                       TRI_V8_STD_STRING(isolate, name), false);

  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      TRI_LogV8Exception(isolate, &tryCatch);  // TODO: could this be the place
                                               // where we lose the information
                                               // about parse errors of scripts?
      return v8::Undefined(isolate);
    } else {
      TRI_GET_GLOBALS();

      v8g->_canceled = true;
    }
  }
  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a named script
////////////////////////////////////////////////////////////////////////////////

JSLoader::eState JSLoader::loadScript(v8::Isolate* isolate, v8::Handle<v8::Context>& context,
                                      std::string const& name, VPackBuilder* builder) {
  v8::HandleScope scope(isolate);
  v8::TryCatch tryCatch;

  findScript(name);

  std::map<std::string, std::string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    // correct the path/name
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "unknown script '" << StringUtils::correctPath(name) << "'";
    return eFailLoad;
  }

  // Enter the newly created execution environment.
  v8::Context::Scope context_scope(context);

  v8::Handle<v8::Value> result =
      TRI_ExecuteJavaScriptString(isolate, context, TRI_V8_STD_STRING(isolate, i->second),
                                  TRI_V8_STD_STRING(isolate, name), false);

  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      TRI_LogV8Exception(isolate, &tryCatch);
    } else {
      TRI_GET_GLOBALS();
      v8g->_canceled = true;
    }
    return eFailExecute;
  }

  // Report the result if there is one:
  if (builder != nullptr) {
    try {
      if (!result.IsEmpty()) {
        TRI_V8ToVPack(isolate, *builder, result, false);
      } else {
        builder->add(VPackValue(VPackValueType::Null));
      }
    } catch (...) {
    }
  }

  return eSuccess;
}
