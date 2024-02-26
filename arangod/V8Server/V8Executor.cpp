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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "V8Executor.h"

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "RestServer/arangod.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

using namespace arangodb;
using namespace arangodb::basics;

V8Executor::V8Executor(size_t id, v8::Isolate* isolate,
                       std::function<void(V8Executor&)> const& cb)
    : _isolate{isolate},
      _id{id},
      _creationStamp{TRI_microtime()},
      _invocations{0},
      // some random delay value to add as an initial garbage collection offset
      // this avoids collecting all contexts at the very same time
      _lastGcStamp{_creationStamp +
                   static_cast<double>(RandomGenerator::interval(0, 60))},
      _invocationsSinceLastGc{0},
      _description{"(none)"},
      _acquired{0.0},
      _hasActiveExternals{true},
      _isInIsolate{false} {
  TRI_ASSERT(_isolate != nullptr);
  TRI_ASSERT(_context.IsEmpty());

  lockAndEnter();
  auto guard = scopeGuard([this]() noexcept { unlockAndExit(); });

  {
    v8::HandleScope scope(_isolate);

    _context.Reset(
        _isolate,
        v8::Context::New(_isolate, nullptr, v8::ObjectTemplate::New(_isolate)));

    if (_context.IsEmpty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_OUT_OF_MEMORY,
          "cannot initalize V8 engine for new executor");
    }

    cb(*this);
  }
}

void V8Executor::lockAndEnter() {
  TRI_ASSERT(_locker == nullptr);
  _locker = std::make_unique<v8::Locker>(_isolate);
  TRI_ASSERT(_locker->IsLocked(_isolate));
  TRI_ASSERT(!_isInIsolate);

  _isolate->Enter();
  _isInIsolate = true;
  TRI_ASSERT(!_isolate->InContext());

  _invocations.fetch_add(1, std::memory_order_relaxed);
  ++_invocationsSinceLastGc;
}

void V8Executor::unlockAndExit() {
  TRI_ASSERT(!_isolate->InContext());
  _isInIsolate = false;
  _isolate->Exit();
  _locker.reset();
}

void V8Executor::setCleaned(double stamp) {
  _lastGcStamp = stamp;
  _invocationsSinceLastGc = 0;
}

void V8Executor::runCodeInContext(std::string_view code,
                                  std::string_view codeDescription) {
  runInContext([&](v8::Isolate* isolate) -> Result {
    try {
      TRI_ExecuteJavaScriptString(isolate, code, codeDescription, false);
    } catch (...) {
      LOG_TOPIC("558dd", WARN, arangodb::Logger::V8)
          << "caught exception during code execution";
      // do not throw from here
    }
    return {};
  });
}

Result V8Executor::runInContext(std::function<Result(v8::Isolate*)> const& cb,
                                bool executeGlobalMethods) {
  TRI_ASSERT(_isolate != nullptr);
  TRI_ASSERT(!_isolate->InContext());

  Result res;

  v8::HandleScope scope(_isolate);

  v8::Handle<v8::Context> context =
      v8::Local<v8::Context>::New(_isolate, _context);
  TRI_ASSERT(!context.IsEmpty());
  {
    v8::Context::Scope contextScope(context);
    TRI_ASSERT(_isolate->InContext());

    std::vector<GlobalExecutorMethods::MethodType> copy;
    if (executeGlobalMethods) {
      // we need to copy the vector of functions so we do not need to hold
      // the lock while we execute them. this avoids potential deadlocks when
      // one of the executed functions itself registers a context method
      std::lock_guard mutexLocker{_globalMethodsLock};
      copy.swap(_globalMethods);
    }
    if (!copy.empty()) {
      // save old security context settings
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
          _isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));

      JavaScriptSecurityContext old(v8g->_securityContext);

      auto restorer = scopeGuard([&]() noexcept {
        // restore old security settings
        v8g->_securityContext = old;
      });

      for (auto const& type : copy) {
        std::string_view code = GlobalExecutorMethods::code(type);

        LOG_TOPIC("fcb75", DEBUG, arangodb::Logger::V8)
            << "executing global context method '" << code << "' for context "
            << _id;

        TRI_ExecuteJavaScriptString(_isolate, code, "global context method",
                                    false);
      }
    }

    res = cb(_isolate);
  }

  TRI_ASSERT(!_isolate->InContext());
  return res;
}

double V8Executor::age() const { return TRI_microtime() - _creationStamp; }

bool V8Executor::shouldBeRemoved(double maxAge, uint64_t maxInvocations) const {
  if (maxAge > 0.0 && age() > maxAge) {
    // context is "too old"
    return true;
  }

  if (maxInvocations > 0 && invocations() >= maxInvocations) {
    // context is used often enough
    return true;
  }

  // re-use the context
  return false;
}

void V8Executor::addGlobalExecutorMethod(
    GlobalExecutorMethods::MethodType type) {
  std::lock_guard mutexLocker{_globalMethodsLock};

  for (auto const& it : _globalMethods) {
    if (it == type) {
      // action is already registered. no need to register it again
      return;
    }
  }

  // insert action into vector
  _globalMethods.emplace_back(type);
}

void V8Executor::handleCancellationCleanup() {
  LOG_TOPIC("e8060", DEBUG, arangodb::Logger::V8)
      << "executing cancelation cleanup context #" << _id;

  runCodeInContext("require('module')._cleanupCancelation();",
                   "context cleanup method");
}
