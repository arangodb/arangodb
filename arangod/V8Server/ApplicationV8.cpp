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

