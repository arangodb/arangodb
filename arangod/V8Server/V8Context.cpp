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

#include "V8Context.h"

#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

using namespace arangodb;
using namespace arangodb::basics;

std::string const GlobalContextMethods::CodeReloadRouting =
    "require(\"@arangodb/actions\").reloadRouting();";

std::string const GlobalContextMethods::CodeReloadAql =
    "try { require(\"@arangodb/aql\").reload(); } catch (err) { }";

std::string const GlobalContextMethods::CodeCollectGarbage =
    "require(\"internal\").wait(0.01, true);";

std::string const GlobalContextMethods::CodeBootstrapCoordinator =
    "require('internal').loadStartup('server/bootstrap/autoload.js').startup();"
    "require('internal').loadStartup('server/bootstrap/routing.js').startup();";

std::string const GlobalContextMethods::CodeWarmupExports =
    "require(\"@arangodb/actions\").warmupExports()";

bool V8Context::addGlobalContextMethod(
    std::string const& method) {
  GlobalContextMethods::MethodType type = GlobalContextMethods::type(method);

  if (type == GlobalContextMethods::MethodType::UNKNOWN) {
    return false;
  }

  MUTEX_LOCKER(mutexLocker, _globalMethodsLock);

  for (auto& it : _globalMethods) {
    if (it == type) {
      // action is already registered. no need to register it again
      return true;
    }
  }

  // insert action into vector
  _globalMethods.emplace_back(type);
  return true;
}

void V8Context::handleGlobalContextMethods() {
  std::vector<GlobalContextMethods::MethodType> copy;

  try {
    // we need to copy the vector of functions so we do not need to hold the
    // lock while we execute them
    // this avoids potential deadlocks when one of the executed functions itself
    // registers a context method

    MUTEX_LOCKER(mutexLocker, _globalMethodsLock);
    copy.swap(_globalMethods);
  } catch (...) {
    // if we failed, we shouldn't have modified _globalMethods yet, so we can
    // try again on the next invocation
    return;
  }

  for (auto& type : copy) {
    std::string const& func = GlobalContextMethods::code(type);

    LOG(DEBUG) << "executing global context method '" << func
               << "' for context " << _id;

    TRI_GET_GLOBALS2(_isolate);
    bool allowUseDatabase = v8g->_allowUseDatabase;
    v8g->_allowUseDatabase = true;

    v8::TryCatch tryCatch;

    TRI_ExecuteJavaScriptString(
        _isolate, _isolate->GetCurrentContext(),
        TRI_V8_STD_STRING2(_isolate, func),
        TRI_V8_ASCII_STRING2(_isolate, "global context method"), false);

    if (tryCatch.HasCaught()) {
      if (tryCatch.CanContinue()) {
        TRI_LogV8Exception(_isolate, &tryCatch);
      }
    }

    v8g->_allowUseDatabase = allowUseDatabase;
  }
}

void V8Context::handleCancelationCleanup() {
  v8::HandleScope scope(_isolate);

  LOG(DEBUG) << "executing cancelation cleanup context " << _id;

  TRI_ExecuteJavaScriptString(
      _isolate, _isolate->GetCurrentContext(),
      TRI_V8_ASCII_STRING2(_isolate,
                           "require('module')._cleanupCancelation();"),
      TRI_V8_ASCII_STRING2(_isolate, "context cleanup method"), false);
}
