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
#include "Basics/FileUtils.h"
#include "Basics/Mutex.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/WriteLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Cluster/v8-cluster.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Rest/HttpRequest.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Scheduler/Scheduler.h"
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

#include <libplatform/libplatform.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the cancelation cleanup
////////////////////////////////////////////////////////////////////////////////

ApplicationV8::ApplicationV8(TRI_server_t* server,
                             arangodb::aql::QueryRegistry* queryRegistry,
                             ApplicationScheduler* scheduler,
                             ApplicationDispatcher* dispatcher)
    : ApplicationFeature("V8"),
      _server(server),
      _queryRegistry(queryRegistry),
      _startupPath(),
      _appPath(),
      _useActions(true),
      _frontendVersionCheck(true),
      _v8Options(""),
      _startupLoader(),
      _vocbase(nullptr),
      _nrInstances(0),
      _contexts(),
      _contextCondition(),
      _freeContexts(),
      _dirtyContexts(),
      _busyContexts(),
      _scheduler(scheduler),
      _dispatcher(dispatcher),
      _definedBooleans(),
      _definedDoubles(),
      _startupFile("server/server.js"),
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
/// @brief runs the garbage collection
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief disables actions
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::disableActions() { _useActions = false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrades the database
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
=======
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
    {
      v8::Context::Scope contextScope(localContext);

      // run upgrade script
      if (!skip) {
        LOG(DEBUG) << "running database init/upgrade";

        auto unuser(_server->_databasesProtector.use());
        auto theLists = _server->_databasesLists.load();
        for (auto& p : theLists->_databases) {
          TRI_vocbase_t* vocbase = p.second;

          // special check script to be run just once in first thread (not in
          // all)
          // but for all databases
          v8::HandleScope scope(isolate);

          v8::Handle<v8::Object> args = v8::Object::New(isolate);
          args->Set(TRI_V8_ASCII_STRING("upgrade"),
                    v8::Boolean::New(isolate, perform));

          localContext->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"),
                                      args);

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
                LOG(FATAL)
                    << "Database '" << vocbase->_name
                    << "' needs upgrade. Please start the server with the "
                       "--upgrade option";
                FATAL_ERROR_EXIT();
              }
            } else {
              LOG(FATAL) << "JavaScript error during server start";
              FATAL_ERROR_EXIT();
            }

            LOG(DEBUG) << "database '" << vocbase->_name
                       << "' init/upgrade done";
          }
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
        LOG(ERR) << "unable to join database threads for database '"
                 << vocbase->_name << "'";
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
      LOG(ERR) << "unable to join database threads for database '"
               << vocbase->_name << "'";
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
  // clang-format off

  options["Javascript Options:help-admin"]
    ("javascript.gc-interval", &_gcInterval,
     "JavaScript request-based garbage collection interval (each x requests)")
    ("javascript.gc-frequency", &_gcFrequency,
     "JavaScript time-based garbage collection frequency (each x seconds)")
    ("javascript.app-path", &_appPath, "directory for Foxx applications")
    ("javascript.startup-directory", &_startupPath,
     "path to the directory containing JavaScript startup scripts")
    ("javascript.v8-options", &_v8Options, "options to pass to v8")
  ;

  options["Hidden Options"]
    ("frontend-version-check", &_frontendVersionCheck,
     "show new versions in the frontend")
  ;

  // clang-format on
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

    LOG(INFO) << "JavaScript using " << StringUtils::join(paths, ", ");
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
    LOG(INFO) << "using V8 options '" << _v8Options << "'";
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
  
  _allocator.reset(new ArrayBufferAllocator);

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
  _gcThread->beginShutdown();

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
  V8Context* context = _freeContexts[pickedContextNr];
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

  std::vector<std::string> files{ "server/initialize.js" };
  
  v8::Isolate::CreateParams createParams;
  createParams.array_buffer_allocator = _allocator.get();
  v8::Isolate* isolate = v8::Isolate::New(createParams);

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
    {
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

      std::string modulesPath = _startupPath + TRI_DIR_SEPARATOR_STR +
                                "server" + TRI_DIR_SEPARATOR_STR + "modules;" +
                                _startupPath + TRI_DIR_SEPARATOR_STR +
                                "common" + TRI_DIR_SEPARATOR_STR + "modules;" +
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
            LOG(FATAL) << "cannot load JavaScript file '" << file << "'";
            FATAL_ERROR_EXIT();
            break;
          case JSLoader::eFailExecute:
            LOG(FATAL) << "error during execution of JavaScript file '" << file
                       << "'";
            FATAL_ERROR_EXIT();
            break;
        }
      }
      TRI_GET_GLOBALS();
      hasActiveExternals = v8g->hasActiveExternals();
    }
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

>>>>>>> f0bb30c3f7865ea1d2c41bb8a110652e0c8a2c41
////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the V8 server
////////////////////////////////////////////////////////////////////////////////

void ApplicationV8::prepareV8Server(size_t i, std::string const& startupFile) {
  // enter context and isolate
  V8Context* context = _contexts[i];

  auto isolate = context->isolate;
  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(isolate);
  isolate->Enter();
  {
    v8::HandleScope handle_scope(isolate);
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::Context::Scope contextScope(localContext);

      // load server startup file
      switch (_startupLoader.loadScript(isolate, localContext, startupFile)) {
        case JSLoader::eSuccess:
          LOG(TRACE) << "loaded JavaScript file '" << startupFile
                     << "'";
          break;
        case JSLoader::eFailLoad:
          LOG(FATAL) << "cannot load JavaScript utilities from file '"
                     << startupFile << "'";
          FATAL_ERROR_EXIT();
          break;
        case JSLoader::eFailExecute:
          LOG(FATAL)
              << "error during execution of JavaScript utilities from file '"
              << startupFile << "'";
          FATAL_ERROR_EXIT();
          break;
      }
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

