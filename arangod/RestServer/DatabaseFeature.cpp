////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "DatabaseFeature.h"

#include "Aql/QueryRegistry.h"
#include "Basics/StringUtils.h"
#include "Basics/ThreadPool.h"
#include "Cluster/v8-cluster.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

DatabaseFeature* DatabaseFeature::DATABASE = nullptr;

DatabaseFeature::DatabaseFeature(ApplicationServer* server)
    : ApplicationFeature(server, "Database"),
      _directory(""),
      _maximalJournalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
      _queryTracking(true),
      _queryCacheMode("off"),
      _queryCacheEntries(128),
      _upgrade(false),
      _skipUpgrade(false),
      _indexThreads(2),
      _defaultWaitForSync(false),
      _forceSyncProperties(true),
      _ignoreDatafileErrors(false),
      _vocbase(nullptr),
      _server(nullptr),
      _replicationApplier(true),
      _disableCompactor(false),
      _checkVersion(false) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Language");
  startsAfter("Logger");
  startsAfter("Random");
  startsAfter("Temp");
  startsAfter("WorkMonitor");
}

void DatabaseFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(Section("database", "Configure the database",
                              "database options", false, false));

  options->addOption("--database.directory", "path to the database directory",
                     new StringParameter(&_directory));

  options->addOption("--database.maximal-journal-size",
                     "default maximal journal size, can be overwritten when "
                     "creating a collection",
                     new UInt64Parameter(&_maximalJournalSize));

  options->addHiddenOption("--database.wait-for-sync",
                           "default wait-for-sync behavior, can be overwritten "
                           "when creating a collection",
                           new BooleanParameter(&_defaultWaitForSync));

  options->addHiddenOption("--database.force-sync-properties",
                           "force syncing of collection properties to disk, "
                           "will use waitForSync value of collection when "
                           "turned off",
                           new BooleanParameter(&_forceSyncProperties));

  options->addOption("--database.upgrade", "perform a database upgrade",
                     new BooleanParameter(&_upgrade));

  options->addHiddenOption("--database.skip-upgrade", "skip a database upgrade",
                           new BooleanParameter(&_skipUpgrade));

  options->addHiddenOption(
      "--database.index-threads",
      "threads to start for parallel background index creation",
      new UInt64Parameter(&_indexThreads));

#warning TODO
#if 0
  (
      _ignoreDatafileErrors(false),
      "database.ignore-datafile-errors", &_ignoreDatafileErrors,
      "load collections even if datafiles may contain errors")

  IGNORE_DATAFILE_ERRORS = _ignoreDatafileErrors;

      _throwCollectionNotLoadedError(false),
  (
      "database.throw-collection-not-loaded-error",
      &_throwCollectionNotLoadedError,
      "throw an error when accessing a collection that is still loading");

    ("server.disable-replication-applier", &_disableReplicationApplier,
           "start with replication applier turned off")

  TRI_SetThrowCollectionNotLoadedVocBase(_throwCollectionNotLoadedError);
#endif

  options->addSection(
      Section("query", "Configure queries", "query options", false, false));

  options->addOption("--query.tracking", "wether to track queries",
                     new BooleanParameter(&_queryTracking));

#warning TODO
#if 0
  // set global query tracking flag
  arangodb::aql::Query::DisableQueryTracking(_disableQueryTracking);

  // configure the query cache
  {
    std::pair<std::string, size_t> cacheProperties{_queryCacheMode,
                                                   _queryCacheMaxResults};
    arangodb::aql::QueryCache::instance()->setProperties(cacheProperties);
  }
#endif

  options->addOption("--query.cache-mode",
                     "mode for the AQL query cache (on, off, demand)",
                     new StringParameter(&_queryCacheMode));

  options->addOption("--query.cache-entries",
                     "maximum number of results in query cache per database",
                     new UInt64Parameter(&_queryCacheEntries));
}

void DatabaseFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::validateOptions";

  if (_directory.empty()) {
    LOG(ERR) << "no database path has been supplied, giving up, please use "
                "the '--database.directory' option";
    abortInvalidParameters();
  }

  // strip trailing separators
  _databasePath = StringUtils::rTrim(_directory, TRI_DIR_SEPARATOR_STR);

  // some arbitrary limit
  if (_indexThreads > 128) {
    _indexThreads = 128;
  }

  if (_maximalJournalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG(ERR) << "invalid value for '--database.maximal-journal-size'. "
                "expected at least "
             << TRI_JOURNAL_MINIMAL_SIZE;
    abortInvalidParameters();
  }

#warning TODO
#if 0
  if (_checkVersion && _upgrade) {
    LOG(ERR) << "cannot specify both '--database.check-version' and "
                "'--database.upgrade'";
    abortInvalidParameters();
  }
#endif

#warning TODO
#if 0
  if (_checkVersion || _upgrade) {
    _replicationApplier = false;
  }
#endif

  if (_upgrade && _skipUpgrade) {
    LOG(ERR) << "cannot specify both '--database.upgrade' and "
                "'--database.skip-upgrade'";
    abortInvalidParameters();
  }

#warning TODO
#if 0
  std::vector<std::string> arguments = _applicationServer->programArguments();

  if (1 < arguments.size()) {
    LOG(FATAL) << "expected at most one database directory, got "
               << arguments.size();
    FATAL_ERROR_EXIT();
  } else if (1 == arguments.size()) {
    _databasePath = arguments[0];
  }

  // disable certain options in unittest or script mode
  OperationMode::server_operation_mode_e mode =
      OperationMode::determineMode(_applicationServer->programOptions());

  if (mode == OperationMode::MODE_SCRIPT ||
      mode == OperationMode::MODE_UNITTESTS) {
    // testing disables authentication
    _disableAuthentication = true;
  }

  OperationMode::server_operation_mode_e mode =
      OperationMode::determineMode(_applicationServer->programOptions());
  if (mode != OperationMode::MODE_SERVER) {
    LOG(FATAL) << "invalid mode. must not specify --console together with "
                  "--daemon or --supervisor";
    FATAL_ERROR_EXIT();
  }

  OperationMode::server_operation_mode_e mode =
      OperationMode::determineMode(_applicationServer->programOptions());
  bool startServer = true;

  if (_applicationServer->programOptions().has("no-server")) {
    startServer = false;
    TRI_ENABLE_STATISTICS = false;
    // --no-server disables all replication appliers
    _disableReplicationApplier = true;
  }
#endif
}

void DatabaseFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  LOG(INFO) << "" << rest::Version::getVerboseVersionString();

  // set singleton
  DATABASE = this;

  // create the server
  TRI_InitServerGlobals();
  _server.reset(new TRI_server_t());

  // create the query registery
  _queryRegistry.reset(new aql::QueryRegistry());
  _server->_queryRegistry = _queryRegistry.get();

  // start the WAL manager (but do not open it yet)
  LOG(TRACE) << "starting WAL logfile manager";

  wal::LogfileManager::initialize(&_databasePath, _server.get());

  if (!wal::LogfileManager::instance()->prepare() ||
      !wal::LogfileManager::instance()->start()) {
    // unable to initialize & start WAL logfile manager
    LOG(FATAL) << "unable to start WAL logfile manager";
    FATAL_ERROR_EXIT();
  }

  KeyGenerator::Initialize();

  // open all databases
  openDatabases();

  // fetch the system database and update the contexts
  updateContexts();
}

void DatabaseFeature::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::stop";

  // clear the query registery
  _server->_queryRegistry = nullptr;

  // close all databases
  closeDatabases();

  // delete the server
  TRI_StopServer(_server.get());

  // clear singleton
  DATABASE = nullptr;

  LOG(INFO) << "ArangoDB has been shut down";
}

void DatabaseFeature::updateContexts() {
  _vocbase = TRI_UseDatabaseServer(_server.get(), TRI_VOC_SYSTEM_DATABASE);

  if (_vocbase == nullptr) {
    LOG(FATAL)
        << "No _system database found in database directory. Cannot start!";
    FATAL_ERROR_EXIT();
  }

  auto queryRegistry = _queryRegistry.get();
  auto server = _server.get();
  auto vocbase = _vocbase;

  V8DealerFeature* dealer = dynamic_cast<V8DealerFeature*>(
      ApplicationServer::lookupFeature("V8Dealer"));

  if (dealer != nullptr) {
    dealer->defineContextUpdate(
        [queryRegistry, server, vocbase](
            v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t i) {
          TRI_InitV8VocBridge(isolate, context, queryRegistry, server, vocbase,
                              i);
          TRI_InitV8Queries(isolate, context);
          TRI_InitV8Cluster(isolate, context);
        },
        vocbase);
  }
}

void DatabaseFeature::shutdownCompactor() {
  auto unuser = _server->_databasesProtector.use();
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
}

void DatabaseFeature::upgradeDatabase() {
#warning TODO
#if 0
  LOG(TRACE) << "starting database init/upgrade";

  // enter context and isolate
  {
    V8Context* context =
        V8DealerFeature::DEALER->enterContext(_vocbase, true, 0);
    v8::HandleScope scope(context->_isolate);
    auto localContext =
        v8::Local<v8::Context>::New(context->_isolate, context->_context);
    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);

      // run upgrade script
      if (!_skipUpgrade) {
        LOG(DEBUG) << "running database init/upgrade";

        auto unuser(_server->_databasesProtector.use());
        auto theLists = _server->_databasesLists.load();

        for (auto& p : theLists->_databases) {
          TRI_vocbase_t* vocbase = p.second;

          // special check script to be run just once in first thread (not in
          // all) but for all databases
          v8::HandleScope scope(context->_isolate);

          v8::Handle<v8::Object> args = v8::Object::New(context->_isolate);
          args->Set(TRI_V8_ASCII_STRING2(context->_isolate, "upgrade"),
                    v8::Boolean::New(context->_isolate, _upgrade));

          localContext->Global()->Set(
              TRI_V8_ASCII_STRING2(context->_isolate, "UPGRADE_ARGS"), args);

          bool ok = TRI_UpgradeDatabase(vocbase, localContext);

          if (!ok) {
            if (localContext->Global()->Has(TRI_V8_ASCII_STRING2(
                    context->_isolate, "UPGRADE_STARTED"))) {
              localContext->Exit();
              if (_upgrade) {
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
    V8DealerFeature::DEALER->exitContext(context);
  }

  if (_upgrade) {
    LOG(INFO) << "database upgrade passed";
    shutdownCompactor();
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  }

  // and return from the context
  LOG(TRACE) << "finished database init/upgrade";
#endif
}

void DatabaseFeature::openDatabases() {
  TRI_vocbase_defaults_t defaults;

  // override with command-line options
  defaults.defaultMaximalSize = _maximalJournalSize;
  defaults.defaultWaitForSync = _defaultWaitForSync;
  defaults.forceSyncProperties = _forceSyncProperties;

#warning TODO
#if 0
  defaults.requireAuthentication = !_disableAuthentication;
  defaults.requireAuthenticationUnixSockets =
      !_disableAuthenticationUnixSockets;
  defaults.authenticateSystemOnly = _authenticateSystemOnly;
#endif

  TRI_ASSERT(_server != nullptr);

  if (_indexThreads > 0) {
    _indexPool.reset(new ThreadPool(_indexThreads, "IndexBuilder"));
  }

#warning appPathm

  bool const iterateMarkersOnOpen =
      !wal::LogfileManager::instance()->hasFoundLastTick();

  int res = TRI_InitServer(
      _server.get(), _indexPool.get(), _databasePath.c_str(), nullptr,
      &defaults, !_replicationApplier, _disableCompactor, iterateMarkersOnOpen);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "cannot create server instance: out of memory";
    FATAL_ERROR_EXIT();
  }

  res = TRI_StartServer(_server.get(), _checkVersion, _upgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    if (_checkVersion && res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
      TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
    }

    LOG(FATAL) << "cannot start server: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "found system database";
}

void DatabaseFeature::closeDatabases() {
  TRI_ASSERT(_server != nullptr);

// cleanup actions
#warning TODO
#if 0
  TRI_CleanupActions();
#endif

  // stop the replication appliers so all replication transactions can end
  if (_replicationApplier) {
    TRI_StopReplicationAppliersServer(_server.get());
  }

  // enforce logfile manager shutdown so we are sure no one else will
  // write to the logs
  wal::LogfileManager::instance()->stop();
}

#if 0

  // and add the feature to the application server
  _applicationServer->addFeature(wal::LogfileManager::instance());

  // run upgrade script
  bool performUpgrade = false;

  if (_applicationServer->programOptions().has("upgrade")) {
    performUpgrade = true;
    // --upgrade disables all replication appliers
    _disableReplicationApplier = true;
    if (_applicationCluster != nullptr) {
      _applicationCluster->disable(); // TODO
    }
  }

  // skip an upgrade even if VERSION is missing

  // .............................................................................
  // prepare the various parts of the Arango server
  // .............................................................................

  KeyGenerator::Initialize();

  // open all databases
  bool const iterateMarkersOnOpen =
      !wal::LogfileManager::instance()->hasFoundLastTick();

  openDatabases(checkVersion, performUpgraden, iterateMarkersOnOpen);

  if (!checkVersion) {
  }

  // fetch the system database
  TRI_vocbase_t* vocbase =
      TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (vocbase == nullptr) {
    LOG(FATAL)
        << "No _system database found in database directory. Cannot start!";
    FATAL_ERROR_EXIT();
  }

  TRI_ASSERT(vocbase != nullptr);

#endif
