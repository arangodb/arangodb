////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

using namespace arangodb;
using namespace arangodb::basics;

std::string const GlobalContextMethods::CodeReloadRouting =
    "require(\"@arangodb/actions\").reloadRouting();";

std::string const GlobalContextMethods::CodeReloadAql =
    "try { require(\"@arangodb/aql\").reload(); } catch (err) { }";

V8Context::V8Context(size_t id, v8::Isolate* isolate)
    : _id(id),
      _isolate(isolate),
      _locker(nullptr),
      _creationStamp(TRI_microtime()),
      _acquired(0.0),
      _description("(none)"),
      _lastGcStamp(0.0),
      _invocations(0),
      _invocationsSinceLastGc(0),
      _hasActiveExternals(false) {}

void V8Context::lockAndEnter() {
  TRI_ASSERT(_isolate != nullptr);
  TRI_ASSERT(_locker == nullptr);
  _locker = new v8::Locker(_isolate);
  _isolate->Enter();

  assertLocked();

  ++_invocations;
  ++_invocationsSinceLastGc;
}

void V8Context::unlockAndExit() {
  assertLocked();

  _isolate->Exit();
  delete _locker;
  _locker = nullptr;

  TRI_ASSERT(!v8::Locker::IsLocked(_isolate));
}

void V8Context::assertLocked() const {
  TRI_ASSERT(_locker != nullptr);
  TRI_ASSERT(_isolate != nullptr);
  TRI_ASSERT(_locker->IsLocked(_isolate));
  TRI_ASSERT(v8::Locker::IsLocked(_isolate));
}

bool V8Context::hasGlobalMethodsQueued() {
  MUTEX_LOCKER(mutexLocker, _globalMethodsLock);
  return !_globalMethods.empty();
}

void V8Context::setCleaned(double stamp) {
  _lastGcStamp = stamp;
  _invocationsSinceLastGc = 0;
}

double V8Context::age() const { return TRI_microtime() - _creationStamp; }

bool V8Context::shouldBeRemoved(double maxAge, uint64_t maxInvocations) const {
  if (maxAge > 0.0 && age() > maxAge) {
    // context is "too old"
    return true;
  }

  if (maxInvocations > 0 && _invocations >= maxInvocations) {
    // context is used often enough
    return true;
  }

  // re-use the context
  return false;
}

bool V8Context::addGlobalContextMethod(std::string const& method) {
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

    LOG_TOPIC("fcb75", DEBUG, arangodb::Logger::V8)
        << "executing global context method '" << func << "' for context " << _id;

    TRI_GET_GLOBALS2(_isolate);

    // save old security context settings
    JavaScriptSecurityContext old(v8g->_securityContext);

    v8g->_securityContext = JavaScriptSecurityContext::createInternalContext();

    try {
      v8::TryCatch tryCatch(_isolate);

      TRI_ExecuteJavaScriptString(_isolate, _isolate->GetCurrentContext(),
                                  TRI_V8_STD_STRING(_isolate, func),
                                  TRI_V8_ASCII_STRING(_isolate,
                                                      "global context method"),
                                  false);

      if (tryCatch.HasCaught()) {
        if (tryCatch.CanContinue()) {
          TRI_LogV8Exception(_isolate, &tryCatch);
        }
      }
    } catch (...) {
      LOG_TOPIC("d0adc", WARN, arangodb::Logger::V8)
          << "caught exception during global context method '" << func << "'";
    }

    // restore old security settings
    v8g->_securityContext = old;
  }
}

void V8Context::handleCancelationCleanup() {
  v8::HandleScope scope(_isolate);

  LOG_TOPIC("e8060", DEBUG, arangodb::Logger::V8)
      << "executing cancelation cleanup context #" << _id;

  try {
    TRI_ExecuteJavaScriptString(
        _isolate, _isolate->GetCurrentContext(),
        TRI_V8_ASCII_STRING(_isolate,
                            "require('module')._cleanupCancelation();"),
        TRI_V8_ASCII_STRING(_isolate, "context cleanup method"), false);
  } catch (...) {
    LOG_TOPIC("558dd", WARN, arangodb::Logger::V8)
        << "caught exception during cancelation cleanup";
    // do not throw from here
  }
}

V8ContextEntryGuard::V8ContextEntryGuard(V8Context* context)
    : _context(context) {
  _context->lockAndEnter();
}

V8ContextEntryGuard::~V8ContextEntryGuard() { _context->unlockAndExit(); }
