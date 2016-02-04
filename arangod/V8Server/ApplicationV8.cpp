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

#include "ApplicationV8.h"

#include "Actions/actions.h"
#include "ApplicationServer/ApplicationServer.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ConditionLocker.h"
#include "Basics/FileUtils.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/WorkMonitor.h"
#include "Basics/WriteLocker.h"
#include "Basics/Logger.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Cluster/v8-cluster.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/DispatcherThread.h"
#include "Rest/HttpRequest.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Scheduler/Scheduler.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-actions.h"
#include "V8Server/v8-dispatcher.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-user-structures.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/server.h"

#include "3rdParty/valgrind/valgrind.h"

#include <libplatform/libplatform.h>
#include <thread>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief reload the routing cache
////////////////////////////////////////////////////////////////////////////////

char const* GlobalContextMethods::CodeReloadRouting =
    "require(\"@arangodb/actions\").reloadRouting();";

////////////////////////////////////////////////////////////////////////////////
/// @brief reload AQL functions
////////////////////////////////////////////////////////////////////////////////

char const* GlobalContextMethods::CodeReloadAql =
    "try { require(\"@arangodb/aql\").reload(); } catch (err) { }";

////////////////////////////////////////////////////////////////////////////////
/// @brief collect garbage
////////////////////////////////////////////////////////////////////////////////

char const* GlobalContextMethods::CodeCollectGarbage =
    "require(\"internal\").wait(0.01, true);";

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstrap coordinator
////////////////////////////////////////////////////////////////////////////////

char const* GlobalContextMethods::CodeBootstrapCoordinator =
    "require('internal').loadStartup('server/bootstrap/autoload.js').startup();"
    "require('internal').loadStartup('server/bootstrap/routing.js').startup();";

////////////////////////////////////////////////////////////////////////////////
/// @brief warmup the exports
////////////////////////////////////////////////////////////////////////////////

char const* GlobalContextMethods::CodeWarmupExports =
    "require(\"@arangodb/actions\").warmupExports()";

////////////////////////////////////////////////////////////////////////////////
/// @brief we'll store deprecated config option values in here
////////////////////////////////////////////////////////////////////////////////

static std::string DeprecatedPath;

static bool DeprecatedOption;

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collector
////////////////////////////////////////////////////////////////////////////////

class V8GcThread : public Thread {
 public:
  explicit V8GcThread(ApplicationV8* applicationV8)
      : Thread("V8GarbageCollector"),
        _applicationV8(applicationV8),
        _lastGcStamp(static_cast<uint64_t>(TRI_microtime())) {}

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief collect garbage in an endless loop (main functon of GC thread)
  //////////////////////////////////////////////////////////////////////////////

  void run() override { _applicationV8->collectGarbage(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the timestamp of the last GC
  //////////////////////////////////////////////////////////////////////////////

  double getLastGcStamp() {
    return static_cast<double>(_lastGcStamp.load(std::memory_order_acquire));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the global GC timestamp
  //////////////////////////////////////////////////////////////////////////////

  void updateGcStamp(double value) {
    _lastGcStamp.store(static_cast<uint64_t>(value), std::memory_order_release);
  }

 private:
  ApplicationV8* _applicationV8;
  std::atomic<uint64_t> _lastGcStamp;
};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global method
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::V8Context::addGlobalContextMethod(
    std::string const& method) {
  GlobalContextMethods::MethodType type = GlobalContextMethods::getType(method);

  if (type == GlobalContextMethods::TYPE_UNKNOWN) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all global methods
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::V8Context::handleGlobalContextMethods() {
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
    char const* func = GlobalContextMethods::getCode(type);
    // all functions are hard-coded, static const char*s
    TRI_ASSERT(func != nullptr);

    LOG(DEBUG) << "executing global context method '" << func
               << "' for context " << _id;

    TRI_GET_GLOBALS();
    bool allowUseDatabase = v8g->_allowUseDatabase;
    v8g->_allowUseDatabase = true;

    v8::TryCatch tryCatch;

    TRI_ExecuteJavaScriptString(
        isolate, isolate->GetCurrentContext(), TRI_V8_ASCII_STRING(func),
        TRI_V8_ASCII_STRING("global context method"), false);

    if (tryCatch.HasCaught()) {
      if (tryCatch.CanContinue()) {
        TRI_LogV8Exception(isolate, &tryCatch);
      }
    }

    v8g->_allowUseDatabase = allowUseDatabase;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the cancelation cleanup
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::V8Context::handleCancelationCleanup() {
  v8::HandleScope scope(isolate);

  LOG(DEBUG) << "executing cancelation cleanup context " << _id;

  TRI_ExecuteJavaScriptString(
      isolate, isolate->GetCurrentContext(),
      TRI_V8_ASCII_STRING("require('module')._cleanupCancelation();"),
      TRI_V8_ASCII_STRING("context cleanup method"), false);
}

ApplicationV8::ApplicationV8(TRI_server_t* server,
                             arangodb::aql::QueryRegistry* queryRegistry,
                             ApplicationScheduler* scheduler,
                             ApplicationDispatcher* dispatcher)
    : ApplicationFeature("V8"),
      _server(server),
      _queryRegistry(queryRegistry),
      _startupPath(),
      _appPath(),
      _devAppPath(),
      _useActions(true),
      _frontendVersionCheck(true),
      _gcInterval(1000),
      _gcFrequency(15.0),
      _v8Options(""),
      _startupLoader(),
      _vocbase(nullptr),
      _nrInstances(0),
      _contexts(),
      _contextCondition(),
      _freeContexts(),
      _dirtyContexts(),
      _busyContexts(),
      _stopping(false),
      _gcThread(nullptr),
      _scheduler(scheduler),
      _dispatcher(dispatcher),
      _definedBooleans(),
      _definedDoubles(),
      _startupFile("server/server.js"),
      _gcFinished(false),
      _platform(nullptr) {
  TRI_ASSERT(_server != nullptr);
}

ApplicationV8::~ApplicationV8() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the concurrency
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::setConcurrency(size_t n) {
  _nrInstances = n;

  _busyContexts.reserve(n);
  _freeContexts.reserve(n);
  _dirtyContexts.reserve(n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the database
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::setVocbase(TRI_vocbase_t* vocbase) { _vocbase = vocbase; }

////////////////////////////////////////////////////////////////////////////////
/// @brief enters a context
////////////////////////////////////////////////////////////////////////////////

ApplicationV8::V8Context* ApplicationV8::enterContext(TRI_vocbase_t* vocbase,
                                                      bool allowUseDatabase,
                                                      ssize_t forceContext) {
  v8::Isolate* isolate = nullptr;
  V8Context* context = nullptr;

  // this is for TESTING / DEBUGGING only
  if (forceContext != -1) {
    size_t id = (size_t)forceContext;

    while (!_stopping) {
      {
        CONDITION_LOCKER(guard, _contextCondition);

        for (auto iter = _freeContexts.begin(); iter != _freeContexts.end();
             ++iter) {
          if ((*iter)->_id == id) {
            context = *iter;
            _freeContexts.erase(iter);
            _busyContexts.emplace(context);
            break;
          }
        }

        if (context != nullptr) {
          break;
        }

        for (auto iter = _dirtyContexts.begin(); iter != _dirtyContexts.end();
             ++iter) {
          if ((*iter)->_id == id) {
            context = *iter;
            _dirtyContexts.erase(iter);
            _busyContexts.emplace(context);
            break;
          }
        }

        if (context != nullptr) {
          break;
        }
      }

      LOG(DEBUG) << "waiting for V8 context " << id << " to become available";
      usleep(100 * 1000);
    }

    if (context == nullptr) {
      return nullptr;
    }

    isolate = context->isolate;
  }

  // look for a free context
  else {
    CONDITION_LOCKER(guard, _contextCondition);

    while (_freeContexts.empty() && !_stopping) {
      LOG(DEBUG) << "waiting for unused V8 context";

      if (!_dirtyContexts.empty()) {
        // we'll use a dirty context in this case
        auto context = _dirtyContexts.back();
        _freeContexts.emplace_back(context);
        _dirtyContexts.pop_back();
      } else {
        auto currentThread =
            arangodb::rest::DispatcherThread::currentDispatcherThread;

        if (currentThread != nullptr) {
          arangodb::rest::DispatcherThread::currentDispatcherThread->block();
        }
        guard.wait();
        if (currentThread != nullptr) {
          arangodb::rest::DispatcherThread::currentDispatcherThread->unblock();
        }
      }
    }

    // in case we are in the shutdown phase, do not enter a context!
    // the context might have been deleted by the shutdown
    if (_stopping) {
      return nullptr;
    }

    LOG(TRACE) << "found unused V8 context";
    TRI_ASSERT(!_freeContexts.empty());

    context = _freeContexts.back();
    TRI_ASSERT(context != nullptr);

    isolate = context->isolate;

    _freeContexts.pop_back();

    // should not fail because we reserved enough space beforehand
    _busyContexts.emplace(context);
  }

  // when we get here, we should have a context and an isolate
  TRI_ASSERT(context != nullptr);
  TRI_ASSERT(isolate != nullptr);

  context->_locker = new v8::Locker(isolate);
  context->isolate->Enter();

  v8::HandleScope scope(isolate);
  auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
  localContext->Enter();

  {
    v8::Context::Scope contextScope(localContext);

    TRI_ASSERT(context->_locker->IsLocked(isolate));
    TRI_ASSERT(v8::Locker::IsLocked(isolate));
    TRI_GET_GLOBALS();

    // initialize the context data
    v8g->_query = nullptr;
    v8g->_vocbase = vocbase;
    v8g->_allowUseDatabase = allowUseDatabase;

    TRI_UseVocBase(vocbase);

    LOG(TRACE) << "entering V8 context " << context->_id;
    context->handleGlobalContextMethods();
  }

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exits a context
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::exitContext(V8Context* context) {
  V8GcThread* gc = dynamic_cast<V8GcThread*>(_gcThread);
  TRI_ASSERT(gc != nullptr);

  LOG(TRACE) << "leaving V8 context " << context->_id;
  double lastGc = gc->getLastGcStamp();

  auto isolate = context->isolate;

  TRI_ASSERT(context->_locker->IsLocked(isolate));
  TRI_ASSERT(v8::Locker::IsLocked(isolate));

  // update data for later garbage collection
  TRI_GET_GLOBALS();
  context->_hasActiveExternals = v8g->hasActiveExternals();
  ++context->_numExecutions;

  TRI_ASSERT(v8g->_vocbase != nullptr);
  // release last recently used vocbase
  TRI_ReleaseVocBase(static_cast<TRI_vocbase_t*>(v8g->_vocbase));

  // check for cancelation requests
  bool const canceled = v8g->_canceled;
  v8g->_canceled = false;

  // check if we need to execute global context methods
  bool runGlobal = false;

  {
    MUTEX_LOCKER(mutexLocker, context->_globalMethodsLock);
    runGlobal = !context->_globalMethods.empty();
  }

  // exit the context
  {
    v8::HandleScope scope(isolate);

    // if the execution was canceled, we need to cleanup
    if (canceled) {
      context->handleCancelationCleanup();
    }

    // run global context methods
    if (runGlobal) {
      TRI_ASSERT(context->_locker->IsLocked(isolate));
      TRI_ASSERT(v8::Locker::IsLocked(isolate));

      context->handleGlobalContextMethods();
    }

    // now really exit
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Exit();
  }
  isolate->Exit();

  delete context->_locker;
  context->_locker = nullptr;

  TRI_ASSERT(!v8::Locker::IsLocked(isolate));

  // reset the context data. garbage collection should be able to run without it
  v8g->_query = nullptr;
  v8g->_vocbase = nullptr;
  v8g->_allowUseDatabase = false;

  LOG(TRACE) << "returned dirty V8 context";

  // default is false
  bool performGarbageCollection = false;

  // postpone garbage collection for standard contexts
  if (context->_lastGcStamp + _gcFrequency < lastGc) {
    LOG(TRACE) << "V8 context has reached GC timeout threshold and will be "
                  "scheduled for GC";
    performGarbageCollection = true;
  } else if (context->_numExecutions >= _gcInterval) {
    LOG(TRACE) << "V8 context has reached maximum number of requests and will "
                  "be scheduled for GC";
    performGarbageCollection = true;
  }

  {
    CONDITION_LOCKER(guard, _contextCondition);

    if (performGarbageCollection && !_freeContexts.empty()) {
      // only add the context to the dirty list if there is at least one other
      // free context
      _dirtyContexts.emplace_back(context);
    } else {
      _freeContexts.emplace_back(context);
    }

    _busyContexts.erase(context);

    guard.broadcast();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global context function to be executed asap
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::addGlobalContextMethod(std::string const& method) {
  bool result = true;
  size_t nrInstances = _nrInstances;

  for (size_t i = 0; i < nrInstances; ++i) {
    try {
      if (!_contexts[i]->addGlobalContextMethod(method)) {
        result = false;
      }
    } catch (...) {
      result = false;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the garbage collection
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::collectGarbage() {
  V8GcThread* gc = dynamic_cast<V8GcThread*>(_gcThread);
  TRI_ASSERT(gc != nullptr);

  // this flag will be set to true if we timed out waiting for a GC signal
  // if set to true, the next cycle will use a reduced wait time so the GC
  // can be performed more early for all dirty contexts. The flag is set
  // to false again once all contexts have been cleaned up and there is nothing
  // more to do
  bool useReducedWait = false;
  bool preferFree = false;

  // the time we'll wait for a signal
  uint64_t const regularWaitTime = (uint64_t)(_gcFrequency * 1000.0 * 1000.0);

  // the time we'll wait for a signal when the previous wait timed out
  uint64_t const reducedWaitTime = (uint64_t)(_gcFrequency * 1000.0 * 200.0);

  while (_stopping == 0) {
    V8Context* context = nullptr;
    bool wasDirty = false;

    {
      bool gotSignal = false;
      preferFree = !preferFree;
      CONDITION_LOCKER(guard, _contextCondition);

      if (_dirtyContexts.empty()) {
        uint64_t waitTime = useReducedWait ? reducedWaitTime : regularWaitTime;

        // we'll wait for a signal or a timeout
        gotSignal = guard.wait(waitTime);
      }

      if (preferFree && !_freeContexts.empty()) {
        context = pickFreeContextForGc();
      }

      if (context == nullptr && !_dirtyContexts.empty()) {
        context = _dirtyContexts.back();
        _dirtyContexts.pop_back();
        if (context->_numExecutions < 50 && !context->_hasActiveExternals) {
          // don't collect this one yet. it doesn't have externals, so there
          // is urge for garbage collection
          _freeContexts.emplace_back(context);
          context = nullptr;
        } else {
          wasDirty = true;
        }
      }

      if (context == nullptr && !preferFree && !gotSignal &&
          !_freeContexts.empty()) {
        // we timed out waiting for a signal, so we have idle time that we can
        // spend on running the GC pro-actively
        // We'll pick one of the free contexts and clean it up
        context = pickFreeContextForGc();
      }

      // there is no context to clean up, probably they all have been cleaned up
      // already. increase the wait time so we don't cycle too much in the GC
      // loop
      // and waste CPU unnecessary
      useReducedWait = (context != nullptr);
    }

    // update last gc time
    double lastGc = TRI_microtime();
    gc->updateGcStamp(lastGc);

    if (context != nullptr) {
      arangodb::CustomWorkStack custom("V8 GC", (uint64_t)context->_id);

      LOG(TRACE) << "collecting V8 garbage in context #" << context->_id
                 << ", numExecutions: " << context->_numExecutions
                 << ", hasActive: " << context->_hasActiveExternals
                 << ", wasDirty: " << wasDirty;
      bool hasActiveExternals = false;
      auto isolate = context->isolate;
      TRI_ASSERT(context->_locker == nullptr);
      context->_locker = new v8::Locker(isolate);
      isolate->Enter();
      {
        v8::HandleScope scope(isolate);

        auto localContext =
            v8::Local<v8::Context>::New(isolate, context->_context);

        localContext->Enter();
        v8::Context::Scope contextScope(localContext);

        TRI_ASSERT(context->_locker->IsLocked(isolate));
        TRI_ASSERT(v8::Locker::IsLocked(isolate));

        TRI_GET_GLOBALS();
        TRI_RunGarbageCollectionV8(isolate, 1.0);
        hasActiveExternals = v8g->hasActiveExternals();

        localContext->Exit();
      }

      isolate->Exit();
      delete context->_locker;
      context->_locker = nullptr;

      // update garbage collection statistics
      context->_hasActiveExternals = hasActiveExternals;
      context->_numExecutions = 0;
      context->_lastGcStamp = lastGc;

      {
        CONDITION_LOCKER(guard, _contextCondition);

        if (wasDirty) {
          _freeContexts.emplace_back(context);
        } else {
          _freeContexts.insert(_freeContexts.begin(), context);
        }
        guard.broadcast();
      }
    } else {
      useReducedWait = false;  // sanity
    }
  }

  _gcFinished = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disables actions
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::disableActions() { _useActions = false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrades the database
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::upgradeDatabase(bool skip, bool perform) {
  LOG(TRACE) << "starting database init/upgrade";

  // enter context and isolate
  V8Context* context = _contexts[0];

  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(context->isolate);
  auto isolate = context->isolate;
  isolate->Enter();
  {
    v8::HandleScope scope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    v8::Context::Scope contextScope(localContext);

    // run upgrade script
    if (!skip) {
      LOG(DEBUG) << "running database init/upgrade";

      auto unuser(_server->_databasesProtector.use());
      auto theLists = _server->_databasesLists.load();
      for (auto& p : theLists->_databases) {
        TRI_vocbase_t* vocbase = p.second;

        // special check script to be run just once in first thread (not in all)
        // but for all databases
        v8::HandleScope scope(isolate);

        v8::Handle<v8::Object> args = v8::Object::New(isolate);
        args->Set(TRI_V8_ASCII_STRING("upgrade"),
                  v8::Boolean::New(isolate, perform));

        localContext->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"), args);

        bool ok = TRI_UpgradeDatabase(vocbase, &_startupLoader, localContext);

        if (!ok) {
          if (localContext->Global()->Has(
                  TRI_V8_ASCII_STRING("UPGRADE_STARTED"))) {
            localContext->Exit();
            if (perform) {
              LOG(FATAL) << "Database '" << vocbase->_name
                         << "' upgrade failed. Please inspect the logs from "
                            "the upgrade procedure";
              FATAL_ERROR_EXIT();
            } else {
              LOG(FATAL) << "Database '" << vocbase->_name
                         << "' needs upgrade. Please start the server with the "
                            "--upgrade option";
              FATAL_ERROR_EXIT();
            }
          } else {
            LOG(FATAL) << "JavaScript error during server start";
            FATAL_ERROR_EXIT();
          }

          LOG(DEBUG) << "database '" << vocbase->_name << "' init/upgrade done";
        }
      }
    }

    // finally leave the context. otherwise v8 will crash with assertion failure
    // when we delete
    // the context locker below
    localContext->Exit();
  }

  isolate->Exit();
  delete context->_locker;
  context->_locker = nullptr;

  if (perform) {
    // issue #391: when invoked with --upgrade, the server will not always shut
    // down
    LOG(INFO) << "database upgrade passed";

    // regular shutdown... wait for all threads to finish

    auto unuser(_server->_databasesProtector.use());
    auto theLists = _server->_databasesLists.load();
    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;

      vocbase->_state = 2;

      int res = TRI_ERROR_NO_ERROR;

      res |= TRI_StopCompactorVocBase(vocbase);
      vocbase->_state = 3;
      res |= TRI_JoinThread(&vocbase->_cleanup);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG(ERR) << "unable to join database threads for database '" << vocbase->_name << "'";
      }
    }

    LOG(INFO) << "finished";
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  } else {
    // and return from the context
    LOG(TRACE) << "finished database init/upgrade";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the version check
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::versionCheck() {
  int result = 1;
  LOG(TRACE) << "starting version check";

  // enter context and isolate
  V8Context* context = _contexts[0];

  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(context->isolate);
  auto isolate = context->isolate;
  isolate->Enter();
  {
    v8::HandleScope scope(isolate);
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    v8::Context::Scope contextScope(localContext);

    // run upgrade script
    LOG(DEBUG) << "running database version check";

    // can do this without a lock as this is the startup

    auto unuser(_server->_databasesProtector.use());
    auto theLists = _server->_databasesLists.load();
    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;

      // special check script to be run just once in first thread (not in all)
      // but for all databases

      int status =
          TRI_CheckDatabaseVersion(vocbase, &_startupLoader, localContext);

      if (status < 0) {
        LOG(FATAL) << "Database version check failed for '" << vocbase->_name
                   << "'. Please inspect the logs from any errors";
        FATAL_ERROR_EXIT();
      } else if (status == 3) {
        result = 3;
      } else if (status == 2 && result == 1) {
        result = 2;
      }
    }

    // issue #391: when invoked with --upgrade, the server will not always shut
    // down
    localContext->Exit();
  }
  isolate->Exit();
  delete context->_locker;
  context->_locker = nullptr;

  // regular shutdown... wait for all threads to finish

  auto unuser(_server->_databasesProtector.use());
  auto theLists = _server->_databasesLists.load();
  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;

    vocbase->_state = 2;

    int res = TRI_ERROR_NO_ERROR;

    res |= TRI_StopCompactorVocBase(vocbase);
    vocbase->_state = 3;
    res |= TRI_JoinThread(&vocbase->_cleanup);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to join database threads for database '" << vocbase->_name << "'";
    }
  }

  if (result == 1) {
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  } else {
    TRI_EXIT_FUNCTION(result, nullptr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the server
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::prepareServer() {
  size_t nrInstances = _nrInstances;

  for (size_t i = 0; i < nrInstances; ++i) {
    prepareV8Server(i, _startupFile);
  }
}

void ApplicationV8::setupOptions(
    std::map<std::string, basics::ProgramOptionsDescription>& options) {
  options["Javascript Options:help-admin"](
      "javascript.gc-interval", &_gcInterval,
      "JavaScript request-based garbage collection interval (each x requests)")(
      "javascript.gc-frequency", &_gcFrequency,
      "JavaScript time-based garbage collection frequency (each x seconds)")(
      "javascript.app-path", &_appPath, "directory for Foxx applications")(
      "javascript.startup-directory", &_startupPath,
      "path to the directory containing JavaScript startup scripts")(
      "javascript.v8-options", &_v8Options, "options to pass to v8");

  options["Hidden Options"]("frontend-version-check", &_frontendVersionCheck,
                            "show new versions in the frontend")(
      "frontend-development-mode", &DeprecatedOption,
      "only here for compatibility")(
      "javascript.dev-app-path", &_devAppPath,
      "directory for Foxx applications (development mode)")

      // deprecated options
      ("javascript.action-directory", &DeprecatedPath,
       "path to the JavaScript action directory (deprecated)")(
          "javascript.modules-path", &DeprecatedPath,
          "one or more directories separated by semi-colons (deprecated)")(
          "javascript.package-path", &DeprecatedPath,
          "one or more directories separated by semi-colons (deprecated)");
}

bool ApplicationV8::prepare() {
  // check the startup path
  if (_startupPath.empty()) {
    LOG(FATAL)
        << "no 'javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  // remove trailing / from path
  _startupPath = StringUtils::rTrim(_startupPath, TRI_DIR_SEPARATOR_STR);

  // dump paths
  {
    std::vector<std::string> paths;

    paths.push_back(std::string("startup '" + _startupPath + "'"));

    if (!_appPath.empty()) {
      paths.push_back(std::string("application '" + _appPath + "'"));
    }

    if (!_devAppPath.empty()) {
      paths.push_back(std::string("dev application '" + _devAppPath + "'"));
    }

    LOG(INFO) << "JavaScript using " << StringUtils::join(paths, ", ").c_str();
  }

  // check whether app-path was specified
  if (_appPath.empty()) {
    LOG(FATAL) << "no value has been specified for --javascript.app-path.";
    FATAL_ERROR_EXIT();
  }

  _startupLoader.setDirectory(_startupPath);
  ServerState::instance()->setJavaScriptPath(_startupPath);

  // add v8 options
  if (!_v8Options.empty()) {
    LOG(INFO) << "using V8 options '" << _v8Options.c_str() << "'";
    v8::V8::SetFlagsFromString(_v8Options.c_str(), (int)_v8Options.size());
  }

#ifdef TRI_FORCE_ARMV6
  std::string const forceARMv6 = "--noenable-armv7";
  v8::V8::SetFlagsFromString(forceARMv6.c_str(), (int)forceARMv6.size());
#endif

  // use a minimum of 1 second for GC
  if (_gcFrequency < 1) {
    _gcFrequency = 1;
  }

  return true;
}

bool ApplicationV8::prepare2() {
  size_t nrInstances = _nrInstances;
  v8::V8::InitializeICU();

  TRI_ASSERT(_platform == nullptr);
  _platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(_platform);
  v8::V8::Initialize();

  v8::V8::SetArrayBufferAllocator(&_bufferAllocator);

  // setup instances
  {
    CONDITION_LOCKER(guard, _contextCondition);
    _contexts = new V8Context*[nrInstances];
  }

  std::vector<std::thread> threads;
  _ok = true;

  for (size_t i = 0; i < nrInstances; ++i) {
    threads.push_back(std::thread(&ApplicationV8::prepareV8InstanceInThread,
                                  this, i, _useActions));
  }
  for (size_t i = 0; i < nrInstances; ++i) {
    threads[i].join();
  }
  return _ok;
}

bool ApplicationV8::start() {
  TRI_ASSERT(_gcThread == nullptr);
  _gcThread = new V8GcThread(this);
  _gcThread->start();

  _gcFinished = false;
  return true;
}

void ApplicationV8::close() {
  _stopping = true;
  _contextCondition.broadcast();

  // unregister all tasks
  if (_scheduler != nullptr && _scheduler->scheduler() != nullptr) {
    _scheduler->scheduler()->unregisterUserTasks();
  }

  // wait for all contexts to finish
  CONDITION_LOCKER(guard, _contextCondition);

  for (size_t n = 0; n < 10 * 5; ++n) {
    if (_busyContexts.empty()) {
      LOG(DEBUG) << "no busy V8 contexts";
      break;
    }

    LOG(DEBUG) << "waiting for " << _busyContexts.size()
               << " busy V8 contexts to finish";

    guard.wait(100000);
  }
}

void ApplicationV8::stop() {
  // send all busy contexts a termate signal
  {
    CONDITION_LOCKER(guard, _contextCondition);

    for (auto& it : _busyContexts) {
      LOG(WARN) << "sending termination signal to V8 context";
      v8::V8::TerminateExecution(it->isolate);
    }
  }

  // wait for one minute
  {
    CONDITION_LOCKER(guard, _contextCondition);

    for (size_t n = 0; n < 10 * 60; ++n) {
      if (_busyContexts.empty()) {
        break;
      }

      guard.wait(100000);
    }
  }

  LOG(DEBUG) << "Waiting for GC Thread to finish action";

  // wait until garbage collector thread is done
  while (!_gcFinished) {
    usleep(10000);
  }

  LOG(DEBUG) << "Commanding GC Thread to terminate";
  // stop GC thread
  _gcThread->shutdown();

  // shutdown all instances
  {
    CONDITION_LOCKER(guard, _contextCondition);

    size_t nrInstances = _nrInstances;

    for (size_t i = 0; i < nrInstances; ++i) {
      shutdownV8Instance(i);
    }

    delete[] _contexts;
  }

  LOG(DEBUG) << "Shutting down V8";

  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();

  TRI_ASSERT(_platform != nullptr);
  delete _platform;
  // delete GC thread after all action threads have been stopped
  delete _gcThread;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which of the free contexts should be picked for the GC
////////////////////////////////////////////////////////////////////////////////

ApplicationV8::V8Context* ApplicationV8::pickFreeContextForGc() {
  int const n = (int)_freeContexts.size();

  if (n == 0) {
    // this is easy...
    return nullptr;
  }

  V8GcThread* gc = dynamic_cast<V8GcThread*>(_gcThread);
  TRI_ASSERT(gc != nullptr);

  V8Context* context = nullptr;

  // we got more than 1 context to clean up, pick the one with the "oldest" GC
  // stamp
  int pickedContextNr =
      -1;  // index of context with lowest GC stamp, -1 means "none"

  for (int i = n - 1; i > 0; --i) {
    // check if there's actually anything to clean up in the context
    if (_freeContexts[i]->_numExecutions < 50 &&
        !_freeContexts[i]->_hasActiveExternals) {
      continue;
    }

    // compare last GC stamp
    if (pickedContextNr == -1 ||
        _freeContexts[i]->_lastGcStamp <=
            _freeContexts[pickedContextNr]->_lastGcStamp) {
      pickedContextNr = i;
    }
  }

  // we now have the context to clean up in pickedContextNr

  if (pickedContextNr == -1) {
    // no context found
    return nullptr;
  }

  // this is the context to clean up
  context = _freeContexts[pickedContextNr];
  TRI_ASSERT(context != nullptr);

  // now compare its last GC timestamp with the last global GC stamp
  if (context->_lastGcStamp + _gcFrequency >= gc->getLastGcStamp()) {
    // no need yet to clean up the context
    return nullptr;
  }

  // we'll pop the context from the vector. the context might be at
  // any position in the vector so we need to move the other elements
  // around
  if (n > 1) {
    for (int i = pickedContextNr; i < n - 1; ++i) {
      _freeContexts[i] = _freeContexts[i + 1];
    }
  }
  _freeContexts.pop_back();

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares a V8 instance
////////////////////////////////////////////////////////////////////////////////

bool ApplicationV8::prepareV8Instance(size_t i, bool useActions) {
  CONDITION_LOCKER(guard, _contextCondition);

  std::vector<std::string> files;

  files.push_back("server/initialize.js");

  v8::Isolate* isolate = v8::Isolate::New();

  V8Context* context = _contexts[i] = new V8Context();

  if (context == nullptr) {
    LOG(FATAL) << "cannot initialize V8 context #" << i;
    FATAL_ERROR_EXIT();
  }

  TRI_ASSERT(context->_locker == nullptr);

  // enter a new isolate
  bool hasActiveExternals = false;
  context->_id = i;
  context->isolate = isolate;
  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(isolate);
  context->isolate->Enter();

  // create the context
  {
    v8::HandleScope handle_scope(isolate);

    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

    v8::Persistent<v8::Context> persistentContext;
    persistentContext.Reset(isolate, v8::Context::New(isolate, 0, global));
    auto localContext = v8::Local<v8::Context>::New(isolate, persistentContext);

    localContext->Enter();
    v8::Context::Scope contextScope(localContext);

    context->_context.Reset(context->isolate, localContext);

    if (context->_context.IsEmpty()) {
      LOG(FATAL) << "cannot initialize V8 engine";
      FATAL_ERROR_EXIT();
    }

    v8::Handle<v8::Object> globalObj = localContext->Global();
    globalObj->Set(TRI_V8_ASCII_STRING("GLOBAL"), globalObj);
    globalObj->Set(TRI_V8_ASCII_STRING("global"), globalObj);
    globalObj->Set(TRI_V8_ASCII_STRING("root"), globalObj);

    TRI_InitV8VocBridge(isolate, this, localContext, _queryRegistry, _server,
                        _vocbase, &_startupLoader, i);
    TRI_InitV8Queries(isolate, localContext);
    TRI_InitV8UserStructures(isolate, localContext);

    TRI_InitV8Cluster(isolate, localContext);
    if (_dispatcher->dispatcher() != nullptr) {
      // don't initialize dispatcher if there is no scheduler (server started
      // with --no-server option)
      TRI_InitV8Dispatcher(isolate, localContext, _vocbase, _scheduler,
                           _dispatcher, this);
    }

    if (useActions) {
      TRI_InitV8Actions(isolate, localContext, _vocbase, this);
    }

    std::string modulesPath = _startupPath + TRI_DIR_SEPARATOR_STR + "server" +
                              TRI_DIR_SEPARATOR_STR + "modules;" +
                              _startupPath + TRI_DIR_SEPARATOR_STR + "common" +
                              TRI_DIR_SEPARATOR_STR + "modules;" +
                              _startupPath + TRI_DIR_SEPARATOR_STR + "node";

    TRI_InitV8Buffer(isolate, localContext);
    TRI_InitV8Conversions(localContext);
    TRI_InitV8Utils(isolate, localContext, _startupPath, modulesPath);
    TRI_InitV8DebugUtils(isolate, localContext, _startupPath, modulesPath);
    TRI_InitV8Shell(isolate, localContext);

    {
      v8::HandleScope scope(isolate);

      TRI_AddGlobalVariableVocbase(isolate, localContext,
                                   TRI_V8_ASCII_STRING("APP_PATH"),
                                   TRI_V8_STD_STRING(_appPath));
      TRI_AddGlobalVariableVocbase(isolate, localContext,
                                   TRI_V8_ASCII_STRING("DEV_APP_PATH"),
                                   TRI_V8_STD_STRING(_devAppPath));
      TRI_AddGlobalVariableVocbase(
          isolate, localContext, TRI_V8_ASCII_STRING("FE_VERSION_CHECK"),
          v8::Boolean::New(isolate, _frontendVersionCheck));

      for (auto j : _definedBooleans) {
        localContext->Global()->ForceSet(TRI_V8_STD_STRING(j.first),
                                         v8::Boolean::New(isolate, j.second),
                                         v8::ReadOnly);
      }

      for (auto j : _definedDoubles) {
        localContext->Global()->ForceSet(TRI_V8_STD_STRING(j.first),
                                         v8::Number::New(isolate, j.second),
                                         v8::ReadOnly);
      }
    }

    // load all init files
    for (auto& file : files) {
      switch (_startupLoader.loadScript(isolate, localContext, file)) {
        case JSLoader::eSuccess:
          LOG(TRACE) << "loaded JavaScript file '" << file << "'";
          break;
        case JSLoader::eFailLoad:
          LOG(FATAL) << "cannot load JavaScript file '" << file
                     << "'";
          FATAL_ERROR_EXIT();
          break;
        case JSLoader::eFailExecute:
          LOG(FATAL) << "error during execution of JavaScript file '"
                     << file << "'";
          FATAL_ERROR_EXIT();
          break;
      }
    }
    TRI_GET_GLOBALS();
    hasActiveExternals = v8g->hasActiveExternals();

    // and return from the context
    localContext->Exit();
  }
  isolate->Exit();
  delete context->_locker;
  context->_locker = nullptr;

  // some random delay value to add as an initial garbage collection offset
  // this avoids collecting all contexts at the very same time
  double const randomWait = fmod(static_cast<double>(TRI_UInt32Random()), 15.0);

  // initialize garbage collection for context
  context->_numExecutions = 0;
  context->_hasActiveExternals = hasActiveExternals;
  context->_lastGcStamp = TRI_microtime() + randomWait;

  LOG(TRACE) << "initialized V8 context #" << i;

  _freeContexts.emplace_back(context);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares a V8 instance, multi-threaded version calling the above
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::prepareV8InstanceInThread(size_t i, bool useAction) {
  if (!prepareV8Instance(i, useAction)) {
    _ok = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the V8 server
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::prepareV8Server(size_t i, std::string const& startupFile) {
  // enter context and isolate
  V8Context* context = _contexts[i];

  auto isolate = context->isolate;
  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(isolate);
  v8::HandleScope scope(isolate);
  isolate->Enter();
  {
    v8::HandleScope handle_scope(isolate);
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    v8::Context::Scope contextScope(localContext);

    // load server startup file
    switch (_startupLoader.loadScript(isolate, localContext, startupFile)) {
      case JSLoader::eSuccess:
        LOG(TRACE) << "loaded JavaScript file '" << startupFile.c_str() << "'";
        break;
      case JSLoader::eFailLoad:
        LOG(FATAL) << "cannot load JavaScript utilities from file '"
                   << startupFile.c_str() << "'";
        FATAL_ERROR_EXIT();
        break;
      case JSLoader::eFailExecute:
        LOG(FATAL)
            << "error during execution of JavaScript utilities from file '"
            << startupFile.c_str() << "'";
        FATAL_ERROR_EXIT();
        break;
    }

    // and return from the context
    localContext->Exit();
  }
  isolate->Exit();
  delete context->_locker;
  context->_locker = nullptr;

  // initialize garbage collection for context
  LOG(TRACE) << "initialized V8 server #" << i;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs a V8 instances
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::shutdownV8Instance(size_t i) {
  LOG(TRACE) << "shutting down V8 context #" << i;

  V8Context* context = _contexts[i];

  auto isolate = context->isolate;
  isolate->Enter();
  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(isolate);
  {
    v8::HandleScope scope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    v8::Context::Scope contextScope(localContext);
    double availableTime = 30.0;

    if (RUNNING_ON_VALGRIND) {
      // running under Valgrind
      availableTime *= 10;
      int tries = 0;

      while (tries++ < 10 &&
             TRI_RunGarbageCollectionV8(isolate, availableTime)) {
        if (tries > 3) {
          LOG(WARN) << "waiting for garbage v8 collection to end";
        }
      }
    } else {
      TRI_RunGarbageCollectionV8(isolate, availableTime);
    }

    TRI_GET_GLOBALS();
    if (v8g != nullptr) {
      if (v8g->_transactionContext != nullptr) {
        delete static_cast<V8TransactionContext*>(v8g->_transactionContext);
        v8g->_transactionContext = nullptr;
      }
      delete v8g;
    }

    localContext->Exit();
  }
  context->_context.Reset();

  isolate->Exit();
  delete context->_locker;
  context->_locker = nullptr;

  isolate->Dispose();

  delete context;

  LOG(TRACE) << "closed V8 context #" << i;
}
