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

#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "Cluster/v8-cluster.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseServerFeature.h"
#include "RestServer/RestServerFeature.h"
#include "RestServer/QueryRegistryFeature.h"
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
      _defaultWaitForSync(false),
      _forceSyncProperties(true),
      _ignoreDatafileErrors(false),
      _throwCollectionNotLoadedError(false),
      _vocbase(nullptr),
      _isInitiallyEmpty(false),
      _replicationApplier(true),
      _disableCompactor(false),
      _checkVersion(false),
      _upgrade(false) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("DatabaseServer");
  startsAfter("LogfileManager");
}

void DatabaseFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");
  
  options->addOldOption("server.disable-replication-applier", "database.replication-applier");

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

  options->addHiddenOption(
      "--database.ignore-datafile-errors",
      "load collections even if datafiles may contain errors",
      new BooleanParameter(&_ignoreDatafileErrors));

  options->addHiddenOption(
      "--database.throw-collection-not-loaded-error",
      "throw an error when accessing a collection that is still loading",
      new BooleanParameter(&_throwCollectionNotLoadedError));

  options->addHiddenOption(
      "--database.replication-applier",
      "switch to enable or disable the replication applier",
      new BooleanParameter(&_replicationApplier));
}

void DatabaseFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;

  if (1 == positionals.size()) {
    _directory = positionals[0];
  } else if (1 < positionals.size()) {
    LOG(FATAL) << "expected at most one database directory, got '"
               << StringUtils::join(positionals, ",") << "'";
    FATAL_ERROR_EXIT();
  }

  if (_directory.empty()) {
    LOG(FATAL) << "no database path has been supplied, giving up, please use "
                  "the '--database.directory' option";
    FATAL_ERROR_EXIT();
  }

  // strip trailing separators
  _databasePath = StringUtils::rTrim(_directory, TRI_DIR_SEPARATOR_STR);

  if (_maximalJournalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG(FATAL) << "invalid value for '--database.maximal-journal-size'. "
                  "expected at least "
               << TRI_JOURNAL_MINIMAL_SIZE;
    FATAL_ERROR_EXIT();
  }
}

void DatabaseFeature::start() {
  LOG(INFO) << "" << rest::Version::getVerboseVersionString();

  // sanity check
  if (_checkVersion && _upgrade) {
    LOG(FATAL) << "cannot specify both '--database.check-version' and "
                  "'--database.auto-upgrade'";
    FATAL_ERROR_EXIT();
  }

  // set singleton
  DATABASE = this;

  // set throw collection not loaded behavior
  TRI_SetThrowCollectionNotLoadedVocBase(_throwCollectionNotLoadedError);

  // init key generator
  KeyGenerator::Initialize();

  // open all databases
  openDatabases();

  if (_isInitiallyEmpty && _checkVersion) {
    LOG(DEBUG) << "checking version on an empty database";
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  }

  // update the contexts
  updateContexts();

  // active deadlock detection in case we're not running in cluster mode
  if (!arangodb::ServerState::instance()->isRunningInCluster()) {
    TRI_EnableDeadlockDetectionDatabasesServer(DatabaseServerFeature::SERVER);
  }
}

void DatabaseFeature::unprepare() {
  // close all databases
  closeDatabases();

  // clear singleton
  DATABASE = nullptr;
}

void DatabaseFeature::updateContexts() {
  _vocbase = TRI_UseDatabaseServer(DatabaseServerFeature::SERVER,
                                   TRI_VOC_SYSTEM_DATABASE);

  if (_vocbase == nullptr) {
    LOG(FATAL)
        << "No _system database found in database directory. Cannot start!";
    FATAL_ERROR_EXIT();
  }

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  TRI_ASSERT(queryRegistry != nullptr);

  auto server = DatabaseServerFeature::SERVER;
  TRI_ASSERT(server != nullptr);

  auto vocbase = _vocbase;

  V8DealerFeature* dealer = 
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  
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

void DatabaseFeature::shutdownCompactor() {
  auto unuser = DatabaseServerFeature::SERVER->_databasesProtector.use();
  auto theLists = DatabaseServerFeature::SERVER->_databasesLists.load();

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

void DatabaseFeature::openDatabases() {
  TRI_vocbase_defaults_t defaults;

  // override with command-line options
  defaults.defaultMaximalSize =
      static_cast<TRI_voc_size_t>(_maximalJournalSize);
  defaults.defaultWaitForSync = _defaultWaitForSync;
  defaults.forceSyncProperties = _forceSyncProperties;

  // get authentication (if available)
  RestServerFeature* rest = 
      ApplicationServer::getFeature<RestServerFeature>("RestServer");

  defaults.requireAuthentication = rest->authentication();
  defaults.requireAuthenticationUnixSockets =
      rest->authenticationUnixSockets();
  defaults.authenticateSystemOnly = rest->authenticationSystemOnly();
  
  bool const iterateMarkersOnOpen =
      !wal::LogfileManager::instance()->hasFoundLastTick(); 

  int res = TRI_InitServer(
      DatabaseServerFeature::SERVER, DatabaseServerFeature::INDEX_POOL,
      _databasePath.c_str(), &defaults, !_replicationApplier, _disableCompactor,
      iterateMarkersOnOpen);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "cannot create server instance: out of memory";
    FATAL_ERROR_EXIT();
  }

  res = TRI_StartServer(DatabaseServerFeature::SERVER, _checkVersion, _upgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    if (_checkVersion && res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
      _isInitiallyEmpty = true;
    } else {
      LOG(FATAL) << "cannot start server: " << TRI_errno_string(res);
      FATAL_ERROR_EXIT();
    }
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "found system database";
}

void DatabaseFeature::closeDatabases() {
  // stop the replication appliers so all replication transactions can end
  if (_replicationApplier) {
    TRI_StopReplicationAppliersServer(DatabaseServerFeature::SERVER);
  }
}
