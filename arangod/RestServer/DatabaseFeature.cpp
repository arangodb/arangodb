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

#include "ApplicationFeatures/LoggerFeature.h"
#include "Basics/StringUtils.h"
#include "Basics/ThreadPool.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

DatabaseFeature::DatabaseFeature(ApplicationServer* server)
    : ApplicationFeature(server, "Database"),
      _directory(""),
      _maximalJournalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
      _queryTracking(true),
      _queryCacheMode("off"),
      _queryCacheEntries(128),
      _checkVersion(false),
      _indexThreads(2),
      _defaultWaitForSync(false),
      _forceSyncProperties(true),
      _vocbase(nullptr),
      _server(nullptr) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Language");
  startsAfter("Logger");
  startsAfter("Random");
  startsAfter("Temp");
  startsAfter("V8Dealer");
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

  options->addHiddenOption("--database.check-version",
                           "checks the versions of the database and exit",
                           new BooleanParameter(&_checkVersion, false));

  options->addHiddenOption(
      "--database.index-threads",
      "threads to start for parallel background index creation",
      new UInt64Parameter(&_indexThreads));

  options->addSection(
      Section("query", "Configure queries", "query options", false, false));

  options->addOption("--query.tracking", "wether to track queries",
                     new BooleanParameter(&_queryTracking));

  options->addOption("--query.cache-mode",
                     "mode for the AQL query cache (on, off, demand)",
                     new StringParameter(&_queryCacheMode));

  options->addOption("--query.cache-entries",
                     "maximum number of results in query cache per database",
                     new UInt64Parameter(&_queryCacheEntries));
}

void DatabaseFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_directory.empty()) {
    LOG(FATAL) << "no database path has been supplied, giving up, please use "
                  "the '--database.directory' option";
    FATAL_ERROR_EXIT();
  }

  // strip trailing separators
  _databasePath = StringUtils::rTrim(_directory, TRI_DIR_SEPARATOR_STR);

  // some arbitrary limit
  if (_indexThreads > 128) {
    _indexThreads = 128;
  }

  if (_maximalJournalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG(FATAL) << "invalid value for '--database.maximal-journal-size'. "
                  "expected at least "
               << TRI_JOURNAL_MINIMAL_SIZE;
    FATAL_ERROR_EXIT();
  }

  if (_checkVersion) {
    ApplicationServer::lookupFeature("Daemon")->disable();
    ApplicationServer::lookupFeature("Dispatcher")->disable();
    ApplicationServer::lookupFeature("Endpoint")->disable();
    ApplicationServer::lookupFeature("Scheduler")->disable();
    ApplicationServer::lookupFeature("Ssl")->disable();
    ApplicationServer::lookupFeature("Supervisor")->disable();

    ApplicationServer::lookupFeature("Shutdown")->enable();

    LoggerFeature* logger = dynamic_cast<LoggerFeature*>(
        ApplicationServer::lookupFeature("Logger"));
    logger->setThreaded(false);
  }
}

void DatabaseFeature::start() {
  // create the server
  TRI_InitServerGlobals();
  _server = new TRI_server_t();

  // initialize the server's write ahead log
  wal::LogfileManager::initialize(&_databasePath, _server);

  // check the version and exit
  if (_checkVersion) {
    checkVersion();
  }

#if 0
  // special treatment for the write-ahead log
  // the log must exist before all other server operations can start
  LOG(TRACE) << "starting WAL logfile manager";

  if (!wal::LogfileManager::instance()->prepare() ||
      !wal::LogfileManager::instance()->start()) {
    // unable to initialize & start WAL logfile manager
    LOG(FATAL) << "unable to start WAL logfile manager";
    FATAL_ERROR_EXIT();
  }

#endif
}

void DatabaseFeature::checkVersion() {
  LOG(TRACE) << "starting WAL logfile manager";

  if (!wal::LogfileManager::instance()->prepare() ||
      !wal::LogfileManager::instance()->start()) {
    // unable to initialize & start WAL logfile manager
    LOG(FATAL) << "unable to start WAL logfile manager";
    FATAL_ERROR_EXIT();
  }

  KeyGenerator::Initialize();

  // open all databases
  bool const iterateMarkersOnOpen =
      !wal::LogfileManager::instance()->hasFoundLastTick();

  openDatabases(true, false, iterateMarkersOnOpen);

  // fetch the system database
  _vocbase = TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (_vocbase == nullptr) {
    LOG(FATAL)
        << "No _system database found in database directory. Cannot start!";
    FATAL_ERROR_EXIT();
  }

#warning TODO fixe queryregistry / _startupLoader
  LOG(WARN) << "HERE";
  {
    aql::QueryRegistry* queryRegistry = nullptr;
    auto server = _server;
    auto vocbase = _vocbase;

    V8DealerFeature::DEALER->updateContexts(
        [&queryRegistry, &server, &vocbase](
            v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t i) {
          TRI_InitV8VocBridge(isolate, context, queryRegistry, server,
                              vocbase, i);
        }, vocbase);
  }

  V8DealerFeature::DEALER->loadJavascript(_vocbase);

  // run version check
  int result = 1;
  LOG(TRACE) << "starting version check";

  // enter context and isolate
  {
    V8Context* context =
        V8DealerFeature::DEALER->enterContext(_vocbase, true, 0);
    v8::HandleScope scope(context->_isolate);
    auto localContext =
        v8::Local<v8::Context>::New(context->_isolate, context->_context);
    localContext->Enter();
    v8::Context::Scope contextScope(localContext);
    auto startupLoader = V8DealerFeature::DEALER->startupLoader();

    // run upgrade script
    LOG(DEBUG) << "running database version check";

    // can do this without a lock as this is the startup
    auto unuser = _server->_databasesProtector.use();
    auto theLists = _server->_databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;

      // special check script to be run just once in first thread (not in all)
      // but for all databases

      int status =
          TRI_CheckDatabaseVersion(vocbase, startupLoader, localContext);

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
    V8DealerFeature::DEALER->exitContext(context);
  }

  // regular shutdown... wait for all threads to finish
  {
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

  if (result == 1) {
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  } else {
    TRI_EXIT_FUNCTION(result, nullptr);
  }
}

void DatabaseFeature::openDatabases(bool checkVersion, bool performUpgrade,
                                    bool iterateMarkersOnOpen) {
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

#warning appPathm  and _disableReplicationApplier
  bool _disableReplicationApplier = false;

  int res = TRI_InitServer(_server, _indexPool.get(), _databasePath.c_str(),
                           nullptr, &defaults, _disableReplicationApplier,
                           iterateMarkersOnOpen);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "cannot create server instance: out of memory";
    FATAL_ERROR_EXIT();
  }

  res = TRI_StartServer(_server, checkVersion, performUpgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    if (checkVersion && res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
      TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
    }

    LOG(FATAL) << "cannot start server: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "found system database";
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
      _applicationCluster->disable();
    }
  }

  // skip an upgrade even if VERSION is missing
  bool skipUpgrade = false;

  if (_applicationServer->programOptions().has("no-upgrade")) {
    skipUpgrade = true;
  }

  // .............................................................................
  // prepare the various parts of the Arango server
  // .............................................................................

  KeyGenerator::Initialize();

  // open all databases
  bool const iterateMarkersOnOpen =
      !wal::LogfileManager::instance()->hasFoundLastTick();

  openDatabases(checkVersion, performUpgrade, iterateMarkersOnOpen);

  if (!checkVersion) {
    if (!wal::LogfileManager::instance()->open()) {
      LOG(FATAL) << "Unable to finish WAL recovery procedure";
      FATAL_ERROR_EXIT();
    }
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

 










void ArangoServer::closeDatabases() {
#warning TODO
#if 0
  TRI_ASSERT(_server != nullptr);

  TRI_CleanupActions();

  // stop the replication appliers so all replication transactions can end
  TRI_StopReplicationAppliersServer(_server);

  // enforce logfile manager shutdown so we are sure no one else will
  // write to the logs
  wal::LogfileManager::instance()->stop();

  TRI_StopServer(_server);

  LOG(INFO) << "ArangoDB has been shut down";
#endif
}

#endif
