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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "DatabaseFeature.h"

#include "Agency/v8-agency.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Cluster/v8-cluster.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CursorRepository.h"
#include "Utils/Events.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/vocbase.h"
#include "VocBase/ticks.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

DatabaseFeature* DatabaseFeature::DATABASE = nullptr;

/// @brief database manager thread main loop
/// the purpose of this thread is to physically remove directories of databases
/// that have been dropped
DatabaseManagerThread::DatabaseManagerThread() : Thread("DatabaseManager") {}

DatabaseManagerThread::~DatabaseManagerThread() { shutdown(); }

void DatabaseManagerThread::run() {
  auto databaseFeature =
      ApplicationServer::getFeature<DatabaseFeature>("Database");
  auto dealer = ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  int cleanupCycles = 0;

  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  while (true) {
    try {
      // check if we have to drop some database
      TRI_vocbase_t* database = nullptr;

      {
        auto unuser(databaseFeature->_databasesProtector.use());
        auto theLists = databaseFeature->_databasesLists.load();

        for (TRI_vocbase_t* vocbase : theLists->_droppedDatabases) {
          if (!vocbase->isDangling()) {
            continue;
          }

          // found a database to delete
          database = vocbase;
          break;
        }
      }

      if (database != nullptr) {
        // found a database to delete, now remove it from the struct
        {
          MUTEX_LOCKER(mutexLocker, databaseFeature->_databasesMutex);

          // Build the new value:
          auto oldLists = databaseFeature->_databasesLists.load();
          decltype(oldLists) newLists = nullptr;
          try {
            newLists = new DatabasesLists();
            newLists->_databases = oldLists->_databases;
            newLists->_coordinatorDatabases = oldLists->_coordinatorDatabases;
            for (TRI_vocbase_t* vocbase : oldLists->_droppedDatabases) {
              if (vocbase != database) {
                newLists->_droppedDatabases.insert(vocbase);
              }
            }
          } catch (...) {
            delete newLists;
            continue;  // try again later
          }

          // Replace the old by the new:
          databaseFeature->_databasesLists = newLists;
          databaseFeature->_databasesProtector.scan();
          delete oldLists;

          // From now on no other thread can possibly see the old
          // TRI_vocbase_t*,
          // note that there is only one DatabaseManager thread, so it is
          // not possible that another thread has seen this very database
          // and tries to free it at the same time!
        }
  
        if (database->type() != TRI_VOCBASE_TYPE_COORDINATOR) {
          // regular database
          // ---------------------------
  
          TRI_ASSERT(!database->isSystem());

          // remove apps directory for database
          auto appPath = dealer->appPath();

          if (database->isOwnAppsDirectory() && !appPath.empty()) {
            std::string path = arangodb::basics::FileUtils::buildFilename(
                arangodb::basics::FileUtils::buildFilename(appPath, "_db"),
                database->name());

            if (TRI_IsDirectory(path.c_str())) {
              LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "removing app directory '" << path
                         << "' of database '" << database->name() << "'";

              TRI_RemoveDirectory(path.c_str());
            }
          }
         
          try { 
            engine->dropDatabase(database);
          } catch (std::exception const& ex) {
            LOG_TOPIC(ERR, Logger::FIXME) << "dropping database '" << database->name() << "' failed: " << ex.what();
          } catch (...) {
            LOG_TOPIC(ERR, Logger::FIXME) << "dropping database '" << database->name() << "' failed";
          }
        }

        delete database;

        // directly start next iteration
      } else {
        if (isStopping()) {
          // done
          break;
        }

        usleep(waitTime());

        // The following is only necessary after a wait:
        auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
        if (queryRegistry != nullptr) {
          queryRegistry->expireQueries();
        }

        auto engineRegistry
          = TraverserEngineRegistryFeature::TRAVERSER_ENGINE_REGISTRY;
        if (engineRegistry != nullptr) {
          engineRegistry->expireEngines();
        }

        // on a coordinator, we have no cleanup threads for the databases
        // so we have to do cursor cleanup here
        if (++cleanupCycles >= 10 &&
            arangodb::ServerState::instance()->isCoordinator()) {
          // note: if no coordinator then cleanupCycles will increase endlessly,
          // but it's only used for the following part
          cleanupCycles = 0;

          auto unuser(databaseFeature->_databasesProtector.use());
          auto theLists = databaseFeature->_databasesLists.load();

          for (auto& p : theLists->_coordinatorDatabases) {
            TRI_vocbase_t* vocbase = p.second;
            TRI_ASSERT(vocbase != nullptr);
            auto cursorRepository = vocbase->cursorRepository();

            try {
              cursorRepository->garbageCollect(false);
            } catch (...) {
            }
          }
        }
      }

    } catch (...) {
    }

    // next iteration
  }
}

DatabaseFeature::DatabaseFeature(ApplicationServer* server)
    : ApplicationFeature(server, "Database"),
      _maximalJournalSize(TRI_JOURNAL_DEFAULT_SIZE),
      _defaultWaitForSync(false),
      _forceSyncProperties(true),
      _ignoreDatafileErrors(false),
      _check30Revisions("true"),
      _throwCollectionNotLoadedError(false),
      _vocbase(nullptr),
      _databasesLists(new DatabasesLists()),
      _isInitiallyEmpty(false),
      _replicationApplier(true),
      _checkVersion(false),
      _upgrade(false) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Authentication");
  startsAfter("CacheManager");
  startsAfter("DatabasePath");
  startsAfter("EngineSelector");
  startsAfter("InitDatabase");
  startsAfter("Scheduler");
  startsAfter("StorageEngine");
}

DatabaseFeature::~DatabaseFeature() {
  // clean up
  auto p = _databasesLists.load();
  delete p;
}

void DatabaseFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addOldOption("server.disable-replication-applier",
                        "database.replication-applier");

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
      new AtomicBooleanParameter(&_throwCollectionNotLoadedError));

  options->addHiddenOption(
      "--database.replication-applier",
      "switch to enable or disable the replication applier",
      new BooleanParameter(&_replicationApplier));

  options->addHiddenOption(
      "--database.check-30-revisions",
      "check _rev values in collections created before 3.1",
      new DiscreteValuesParameter<StringParameter>(
          &_check30Revisions,
          std::unordered_set<std::string>{"true", "false", "fail"}));

  // the following option was removed in 3.2 
  // index-creation is now automatically parallelized via the Boost ASIO thread pool
  options->addObsoleteOption(
      "--database.index-threads",
      "threads to start for parallel background index creation", true);
  
  // the following options were removed in 3.2
  options->addObsoleteOption("--database.revision-cache-chunk-size", 
      "chunk size (in bytes) for the document revisions cache", true);
  options->addObsoleteOption("--database.revision-cache-target-size", 
      "total target size (in bytes) for the document revisions cache", true);
}

void DatabaseFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_maximalJournalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for '--database.maximal-journal-size'. "
                  "expected at least "
               << TRI_JOURNAL_MINIMAL_SIZE;
    FATAL_ERROR_EXIT();
  }

  // sanity check
  if (_checkVersion && _upgrade) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "cannot specify both '--database.check-version' and "
                  "'--database.auto-upgrade'";
    FATAL_ERROR_EXIT();
  }
}

void DatabaseFeature::prepare() {}

void DatabaseFeature::start() {
  // set singleton
  DATABASE = this;

  // init key generator
  KeyGenerator::Initialize();

  verifyAppPaths();

  // scan all databases
  VPackBuilder builder;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->getDatabases(builder);

  TRI_ASSERT(builder.slice().isArray());

  int res = iterateDatabases(builder.slice());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not iterate over all databases: "
               << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  if (systemDatabase() == nullptr) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "No _system database found in database directory. Cannot start!";
    FATAL_ERROR_EXIT();
  }

  // start database manager thread
  _databaseManager.reset(new DatabaseManagerThread);

  if (!_databaseManager->start()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not start database manager thread";
    FATAL_ERROR_EXIT();
  }

  // activate deadlock detection in case we're not running in cluster mode
  if (!arangodb::ServerState::instance()->isRunningInCluster()) {
    enableDeadlockDetection();
  }

  // update all v8 contexts
  updateContexts();
}

// signal to all databases that active cursors can be wiped
// this speeds up the actual shutdown because no waiting is necessary
// until the cursors happen to free their underlying transactions
void DatabaseFeature::beginShutdown() {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);

    // throw away all open cursors in order to speed up shutdown
    vocbase->cursorRepository()->garbageCollect(true);
  }
}

void DatabaseFeature::stop() {}

void DatabaseFeature::unprepare() {
  // close all databases
  closeDatabases();

  // delete the database manager thread
  if (_databaseManager != nullptr) {
    _databaseManager->beginShutdown();

    while (_databaseManager->isRunning()) {
      usleep(5000);
    }
  }

  try {
    closeDroppedDatabases();
  } catch (...) {
    // we're in the shutdown... simply ignore any errors produced here
  }

  _databaseManager.reset();

  try {
    // closeOpenDatabases() can throw, but we're in a dtor
    closeOpenDatabases();
  } catch (...) {
  }

  // clear singleton
  DATABASE = nullptr;
}

/// @brief will be called when the recovery phase has run
/// this will call the engine-specific recoveryDone() procedures
/// and will execute engine-unspecific operations (such as starting
/// the replication appliers) for all databases
void DatabaseFeature::recoveryDone() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);

    // execute the engine-specific callbacks on successful recovery
    engine->recoveryDone(vocbase);

    // start the replication applier, which is engine-unspecific
    TRI_ASSERT(vocbase->replicationApplier() != nullptr);

    if (vocbase->replicationApplier()->_configuration._autoStart) {
      if (!_replicationApplier) {
        LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "replication applier explicitly deactivated for database '"
                  << vocbase->name() << "'";
      } else {
        int res = vocbase->replicationApplier()->start(0, false, 0);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "unable to start replication applier for database '"
                    << vocbase->name() << "': " << TRI_errno_string(res);
        }
      }
    }
  }
}

/// @brief create a new database
int DatabaseFeature::createDatabaseCoordinator(TRI_voc_tick_t id,
                                               std::string const& name,
                                               TRI_vocbase_t*& result) {
  result = nullptr;

  if (!TRI_vocbase_t::IsAllowedName(true, name)) {
    events::CreateDatabase(name, TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  MUTEX_LOCKER(mutexLocker, _databaseCreateLock);

  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    auto it = theLists->_coordinatorDatabases.find(name);
    if (it != theLists->_coordinatorDatabases.end()) {
      // name already in use
      events::CreateDatabase(name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
      return TRI_ERROR_ARANGO_DUPLICATE_NAME;
    }
  }

  // name not yet in use, release the read lock
  auto vocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_COORDINATOR, id, name);

  try {
    vocbase->addReplicationApplier(TRI_CreateReplicationApplier(vocbase.get()));
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // increase reference counter
  vocbase->use();

  {
    MUTEX_LOCKER(mutexLocker, _databasesMutex);
    auto oldLists = _databasesLists.load();
    decltype(oldLists) newLists = nullptr;
    try {
      newLists = new DatabasesLists(*oldLists);
      newLists->_coordinatorDatabases.emplace(name, vocbase.get());
    } catch (...) {
      delete newLists;
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    _databasesLists = newLists;
    _databasesProtector.scan();
    delete oldLists;
  }

  result = vocbase.release();
  events::CreateDatabase(name, TRI_ERROR_NO_ERROR);
  return TRI_ERROR_NO_ERROR;
}

/// @brief create a new database
int DatabaseFeature::createDatabase(TRI_voc_tick_t id, std::string const& name,
                                    TRI_vocbase_t*& result) {
  result = nullptr;

  if (!TRI_vocbase_t::IsAllowedName(false, name)) {
    events::CreateDatabase(name, TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  if (id == 0) {
    id = TRI_NewTickServer();
  }

  std::unique_ptr<TRI_vocbase_t> vocbase;
  VPackBuilder builder;

  // create database in storage engine
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  // the create lock makes sure no one else is creating a database while we're
  // inside
  // this function
  MUTEX_LOCKER(mutexLocker, _databaseCreateLock);
  {
    {
      auto unuser(_databasesProtector.use());
      auto theLists = _databasesLists.load();

      auto it = theLists->_databases.find(name);
      if (it != theLists->_databases.end()) {
        // name already in use
        events::CreateDatabase(name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
        return TRI_ERROR_ARANGO_DUPLICATE_NAME;
      }
    }

    builder.openObject();
    builder.add("database", VPackValue(id));
    builder.add("id", VPackValue(std::to_string(id)));
    builder.add("name", VPackValue(name));
    builder.close();

    // createDatabase must return a valid database or throw
    vocbase.reset(engine->createDatabase(id, builder.slice()));

    TRI_ASSERT(vocbase != nullptr);

    try {
      vocbase->addReplicationApplier(
          TRI_CreateReplicationApplier(vocbase.get()));
    } catch (std::exception const& ex) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "initializing replication applier for database '"
                 << vocbase->name() << "' failed: " << ex.what();
      FATAL_ERROR_EXIT();
    }

    // enable deadlock detection
    vocbase->_deadlockDetector.enabled(
        !arangodb::ServerState::instance()->isRunningInCluster());

    // create application directories
    V8DealerFeature* dealer =
        ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
    auto appPath = dealer->appPath();

    // create app directory for database if it does not exist
    int res = createApplicationDirectory(name, appPath);

    if (! engine->inRecovery()) {
      // starts compactor etc.
      engine->recoveryDone(vocbase.get());

      // start the replication applier
      if (_replicationApplier &&
          vocbase->replicationApplier()->_configuration._autoStart) {
        res = vocbase->replicationApplier()->start(0, false, 0);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "unable to start replication applier for database '"
                    << name << "': " << TRI_errno_string(res);
        }
      }

      // increase reference counter
      bool result = vocbase->use();
      TRI_ASSERT(result);
    }

    {
      MUTEX_LOCKER(mutexLocker, _databasesMutex);
      auto oldLists = _databasesLists.load();
      decltype(oldLists) newLists = nullptr;
      try {
        newLists = new DatabasesLists(*oldLists);
        newLists->_databases.insert(std::make_pair(name, vocbase.get()));
      } catch (...) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Out of memory for putting new database into list!";
        // This is bad, but at least we do not crash!
      }
      if (newLists != nullptr) {
        _databasesLists = newLists;
        _databasesProtector.scan();
        delete oldLists;
      }
    }
  }  // release _databaseCreateLock

  // write marker into log
  int res = TRI_ERROR_NO_ERROR;

  if (!engine->inRecovery()) {
    res = engine->writeCreateDatabaseMarker(id, builder.slice());
  }

  result = vocbase.release();
  events::CreateDatabase(name, res);

  return res;
}

/// @brief drop coordinator database
int DatabaseFeature::dropDatabaseCoordinator(TRI_voc_tick_t id, bool force) {
  int res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;

  MUTEX_LOCKER(mutexLocker, _databasesMutex);
  auto oldLists = _databasesLists.load();
  decltype(oldLists) newLists = nullptr;
  TRI_vocbase_t* vocbase = nullptr;
  try {
    newLists = new DatabasesLists(*oldLists);

    for (auto it = newLists->_coordinatorDatabases.begin();
         it != newLists->_coordinatorDatabases.end(); it++) {
      vocbase = it->second;

      if (vocbase->id() == id &&
          (force || vocbase->name() != TRI_VOC_SYSTEM_DATABASE)) {
        newLists->_droppedDatabases.emplace(vocbase);
        newLists->_coordinatorDatabases.erase(it);
        break;
      }
      vocbase = nullptr;
    }
  } catch (...) {
    delete newLists;
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  if (vocbase != nullptr) {
    _databasesLists = newLists;
    _databasesProtector.scan();
    delete oldLists;

    if (vocbase->markAsDropped()) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "dropping coordinator database '" << vocbase->name() << "'";
      res = TRI_ERROR_NO_ERROR;
    }
  } else {
    delete newLists;
  }

  events::DropDatabase(vocbase == nullptr ? "" : vocbase->name(), res);
  return res;
}

/// @brief drop database
int DatabaseFeature::dropDatabase(std::string const& name, bool waitForDeletion,
                                  bool removeAppsDirectory) {
  if (name == TRI_VOC_SYSTEM_DATABASE) {
    // prevent deletion of system database
    return TRI_ERROR_FORBIDDEN;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_voc_tick_t id = 0;
  int res;
  {
    MUTEX_LOCKER(mutexLocker, _databasesMutex);

    auto oldLists = _databasesLists.load();
    decltype(oldLists) newLists = nullptr;
    TRI_vocbase_t* vocbase = nullptr;
    try {
      newLists = new DatabasesLists(*oldLists);

      auto it = newLists->_databases.find(name);
      if (it == newLists->_databases.end()) {
        // not found
        delete newLists;
        events::DropDatabase(name, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
        return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
      } else {
        vocbase = it->second;
        id = vocbase->id();
        // mark as deleted
        TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);

        newLists->_databases.erase(it);
        newLists->_droppedDatabases.insert(vocbase);
      }
    } catch (...) {
      delete newLists;
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(id != 0);

    _databasesLists = newLists;
    _databasesProtector.scan();
    delete oldLists;
       
    TRI_ASSERT(!vocbase->isSystem()); 
    bool result = vocbase->markAsDropped();
    TRI_ASSERT(result);

    vocbase->setIsOwnAppsDirectory(removeAppsDirectory);

    // invalidate all entries for the database
#if USE_PLAN_CACHE
    arangodb::aql::PlanCache::instance()->invalidate(vocbase);
#endif
    arangodb::aql::QueryCache::instance()->invalidate(vocbase);

    engine->prepareDropDatabase(vocbase, !engine->inRecovery(), res);
  }
  // must not use the database after here, as it may now be
  // deleted by the DatabaseManagerThread!

  if (res == TRI_ERROR_NO_ERROR && waitForDeletion) {
    engine->waitUntilDeletion(id, true, res);
  }

  events::DropDatabase(name, res);
  return res;
}

/// @brief drops an existing database
int DatabaseFeature::dropDatabase(TRI_voc_tick_t id, bool waitForDeletion,
                                  bool removeAppsDirectory) {
  std::string name;

  // find database by name
  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;

      if (vocbase->id() == id) {
        name = vocbase->name();
        break;
      }
    }
  }

  // and call the regular drop function
  return dropDatabase(name, waitForDeletion, removeAppsDirectory);
}

std::vector<TRI_voc_tick_t> DatabaseFeature::getDatabaseIdsCoordinator(
    bool includeSystem) {
  std::vector<TRI_voc_tick_t> ids;
  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_coordinatorDatabases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);

      if (includeSystem || vocbase->name() != TRI_VOC_SYSTEM_DATABASE) {
        ids.emplace_back(vocbase->id());
      }
    }
  }

  return ids;
}

std::vector<TRI_voc_tick_t> DatabaseFeature::getDatabaseIds(
    bool includeSystem) {
  std::vector<TRI_voc_tick_t> ids;

  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase->isDropped()) {
        continue;
      }
      if (includeSystem || vocbase->name() != TRI_VOC_SYSTEM_DATABASE) {
        ids.emplace_back(vocbase->id());
      }
    }
  }

  return ids;
}

/// @brief return the list of all database names
std::vector<std::string> DatabaseFeature::getDatabaseNames() {
  std::vector<std::string> names;

  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase->isDropped()) {
        continue;
      }
      names.emplace_back(vocbase->name());
    }
  }

  std::sort(
      names.begin(), names.end(),
      [](std::string const& l, std::string const& r) -> bool { return l < r; });

  return names;
}

/// @brief return the list of all database names for a user
std::vector<std::string> DatabaseFeature::getDatabaseNamesForUser(
    std::string const& username) {
  std::vector<std::string> names;

  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase->isDropped()) {
        continue;
      }

      auto authentication = FeatureCacheFeature::instance()->authenticationFeature();
      auto level = authentication->canUseDatabase(username, vocbase->name());

      if (level == AuthLevel::NONE) {
        continue;
      }

      names.emplace_back(vocbase->name());
    }
  }

  std::sort(
      names.begin(), names.end(),
      [](std::string const& l, std::string const& r) -> bool { return l < r; });

  return names;
}

void DatabaseFeature::useSystemDatabase() {
  TRI_vocbase_t* result = useDatabase(TRI_VOC_SYSTEM_DATABASE);
  TRI_ASSERT(result != nullptr);
}

/// @brief get a coordinator database by its id
/// this will increase the reference-counter for the database
TRI_vocbase_t* DatabaseFeature::useDatabaseCoordinator(TRI_voc_tick_t id) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_coordinatorDatabases) {
    TRI_vocbase_t* vocbase = p.second;

    if (vocbase->id() == id) {
      bool result = vocbase->use();

      // if we got here, no one else can have deleted the database
      TRI_ASSERT(result == true);
      return vocbase;
    }
  }
  return nullptr;
}

TRI_vocbase_t* DatabaseFeature::useDatabaseCoordinator(
    std::string const& name) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  auto it = theLists->_coordinatorDatabases.find(name);

  if (it != theLists->_coordinatorDatabases.end()) {
    TRI_vocbase_t* vocbase = it->second;
    vocbase->use();
    return vocbase;
  }

  return nullptr;
}

TRI_vocbase_t* DatabaseFeature::useDatabase(std::string const& name) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  auto it = theLists->_databases.find(name);

  if (it != theLists->_databases.end()) {
    TRI_vocbase_t* vocbase = it->second;
    if (vocbase->use()) {
      return vocbase;
    }
  }

  return nullptr;
}

TRI_vocbase_t* DatabaseFeature::useDatabase(TRI_voc_tick_t id) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;

    if (vocbase->id() == id) {
      if (vocbase->use()) {
        return vocbase;
      }
      break;
    }
  }

  return nullptr;
}

/// @brief lookup a database by its name, not increasing its reference count
TRI_vocbase_t* DatabaseFeature::lookupDatabaseCoordinator(
    std::string const& name) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  auto it = theLists->_coordinatorDatabases.find(name);

  if (it != theLists->_coordinatorDatabases.end()) {
    TRI_vocbase_t* vocbase = it->second;
    return vocbase;
  }

  return nullptr;
}

/// @brief lookup a database by its name, not increasing its reference count
TRI_vocbase_t* DatabaseFeature::lookupDatabase(std::string const& name) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;

    if (name == vocbase->name()) {
      return vocbase;
    }
  }

  return nullptr;
}

void DatabaseFeature::enumerateDatabases(std::function<void(TRI_vocbase_t*)> func) {
  if (ServerState::instance()->isCoordinator()) {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();
    
    for (auto& p : theLists->_coordinatorDatabases) {
      TRI_vocbase_t* vocbase = p.second;
      // iterate over all databases
      TRI_ASSERT(vocbase != nullptr);
      TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR);
      func(vocbase);
    }
  } else {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();
    
    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      // iterate over all databases
      TRI_ASSERT(vocbase != nullptr);
      TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);
      func(vocbase);
    }
  }
}

void DatabaseFeature::updateContexts() {
  TRI_ASSERT(_vocbase != nullptr);

  useSystemDatabase();

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  TRI_ASSERT(queryRegistry != nullptr);

  auto vocbase = _vocbase;

  V8DealerFeature* dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  dealer->defineContextUpdate(
      [queryRegistry, vocbase](v8::Isolate* isolate,
                               v8::Handle<v8::Context> context, size_t i) {
        TRI_InitV8VocBridge(isolate, context, queryRegistry, vocbase, i);
        TRI_InitV8Queries(isolate, context);
        TRI_InitV8Cluster(isolate, context);
        TRI_InitV8Agency(isolate, context);
      
        StorageEngine* engine = EngineSelectorFeature::ENGINE; 
        TRI_ASSERT(engine != nullptr); // Engine not loaded. Startup broken
        engine->addV8Functions();
      },
      vocbase);
}

void DatabaseFeature::closeDatabases() {
  // stop the replication appliers so all replication transactions can end
  if (_replicationApplier) {
    MUTEX_LOCKER(mutexLocker,
                 _databasesMutex);  // Only one should do this at a time
    // No need for the thread protector here, because we have the mutex

    for (auto& p : _databasesLists.load()->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);
      if (vocbase->replicationApplier() != nullptr) {
        vocbase->replicationApplier()->stop(false, true);
      }
    }
  }
}

/// @brief close all opened databases
void DatabaseFeature::closeOpenDatabases() {
  MUTEX_LOCKER(mutexLocker,
               _databasesMutex);  // Only one should do this at a time
  // No need for the thread protector here, because we have the mutex
  // Note however, that somebody could still read the lists concurrently,
  // therefore we first install a new value, call scan() on the protector
  // and only then really destroy the vocbases:

  // Build the new value:
  auto oldList = _databasesLists.load();
  decltype(oldList) newList = nullptr;
  try {
    newList = new DatabasesLists();
    newList->_droppedDatabases = _databasesLists.load()->_droppedDatabases;
  } catch (...) {
    delete newList;
    throw;
  }

  // Replace the old by the new:
  _databasesLists = newList;
  _databasesProtector.scan();

  // Now it is safe to destroy the old databases and the old lists struct:
  for (auto& p : oldList->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);
    vocbase->shutdown();

    delete vocbase;
  }

  for (auto& p : oldList->_coordinatorDatabases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR);

    delete vocbase;
  }

  delete oldList;  // Note that this does not delete the TRI_vocbase_t pointers!
}

/// @brief create base app directory
int DatabaseFeature::createBaseApplicationDirectory(std::string const& appPath,
                                                    std::string const& type) {
  int res = TRI_ERROR_NO_ERROR;
  std::string path = arangodb::basics::FileUtils::buildFilename(appPath, type);

  if (!TRI_IsDirectory(path.c_str())) {
    std::string errorMessage;
    long systemError;
    res = TRI_CreateDirectory(path.c_str(), systemError, errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "created base application directory '" << path << "'";
    } else {
      if ((res != TRI_ERROR_FILE_EXISTS) || (!TRI_IsDirectory(path.c_str()))) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to create base application directory "
                 << errorMessage;
      } else {
        LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "someone else created base application directory '" << path
                  << "'";
        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  return res;
}

/// @brief create app subdirectory for a database
int DatabaseFeature::createApplicationDirectory(std::string const& name,
                                                std::string const& basePath) {
  if (basePath.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  std::string const path = basics::FileUtils::buildFilename(
      basics::FileUtils::buildFilename(basePath, "_db"), name);
  int res = TRI_ERROR_NO_ERROR;
  if (!TRI_IsDirectory(path.c_str())) {
    long systemError;
    std::string errorMessage;
    res = TRI_CreateRecursiveDirectory(path.c_str(), systemError, errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "created application directory '" << path
                 << "' for database '" << name << "'";
    } else if (res == TRI_ERROR_FILE_EXISTS) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "unable to create application directory '" << path
                << "' for database '" << name << "': " << errorMessage;
      res = TRI_ERROR_NO_ERROR;
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to create application directory '" << path
               << "' for database '" << name << "': " << errorMessage;
    }
  }

  return res;
}

/// @brief iterate over all databases in the databases directory and open them
int DatabaseFeature::iterateDatabases(VPackSlice const& databases) {
  V8DealerFeature* dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  std::string const appPath = dealer->appPath();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  int res = TRI_ERROR_NO_ERROR;

  // open databases in defined order
  MUTEX_LOCKER(mutexLocker, _databasesMutex);

  auto oldLists = _databasesLists.load();
  auto newLists = new DatabasesLists(*oldLists);

  try {
    for (auto const& it : VPackArrayIterator(databases)) {
      TRI_ASSERT(it.isObject());

      LOG_TOPIC(TRACE, Logger::FIXME) << "processing database: " << it.toJson();

      VPackSlice deleted = it.get("deleted");
      if (deleted.isBoolean() && deleted.getBoolean()) {
        // ignore deleted databases here
        continue;
      }

      std::string const databaseName = it.get("name").copyString();

      // create app directory for database if it does not exist
      res = createApplicationDirectory(databaseName, appPath);

      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }

      // open the database and scan collections in it

      // try to open this database
      TRI_vocbase_t* database = engine->openDatabase(it, _upgrade);

      try {
        database->addReplicationApplier(TRI_CreateReplicationApplier(database));
      } catch (std::exception const& ex) {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "initializing replication applier for database '"
                   << database->name() << "' failed: " << ex.what();
        FATAL_ERROR_EXIT();
      }

      if (databaseName == TRI_VOC_SYSTEM_DATABASE) {
        // found the system database
        TRI_ASSERT(_vocbase == nullptr);
        _vocbase = database;
      }

      newLists->_databases.insert(std::make_pair(database->name(), database));
    }
  } catch (std::exception const& ex) {
    delete newLists;

    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "cannot start database: " << ex.what();
    FATAL_ERROR_EXIT();
  } catch (...) {
    delete newLists;

    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "cannot start database: unknown exception";
    FATAL_ERROR_EXIT();
  }

  _databasesLists = newLists;
  _databasesProtector.scan();
  delete oldLists;

  return res;
}

/// @brief close all dropped databases
void DatabaseFeature::closeDroppedDatabases() {
  MUTEX_LOCKER(mutexLocker, _databasesMutex);

  // No need for the thread protector here, because we have the mutex
  // Note however, that somebody could still read the lists concurrently,
  // therefore we first install a new value, call scan() on the protector
  // and only then really destroy the vocbases:

  // Build the new value:
  auto oldList = _databasesLists.load();
  decltype(oldList) newList = nullptr;
  try {
    newList = new DatabasesLists();
    newList->_databases = _databasesLists.load()->_databases;
    newList->_coordinatorDatabases =
        _databasesLists.load()->_coordinatorDatabases;
  } catch (...) {
    delete newList;
    throw;
  }

  // Replace the old by the new:
  _databasesLists = newList;
  _databasesProtector.scan();

  // Now it is safe to destroy the old dropped databases and the old lists
  // struct:
  for (TRI_vocbase_t* vocbase : oldList->_droppedDatabases) {
    TRI_ASSERT(vocbase != nullptr);

    if (vocbase->type() == TRI_VOCBASE_TYPE_NORMAL) {
      vocbase->shutdown();
      delete vocbase;
    } else if (vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
      delete vocbase;
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unknown database type " << vocbase->type() << " "
               << vocbase->name() << " - close doing nothing.";
    }
  }

  delete oldList;  // Note that this does not delete the TRI_vocbase_t pointers!
}

void DatabaseFeature::verifyAppPaths() {
  // create shared application directory js/apps
  V8DealerFeature* dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  auto appPath = dealer->appPath();

  if (!appPath.empty() && !TRI_IsDirectory(appPath.c_str())) {
    long systemError;
    std::string errorMessage;
    int res = TRI_CreateRecursiveDirectory(appPath.c_str(), systemError,
                                           errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "created --javascript.app-path directory '" << appPath
                << "'";
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to create --javascript.app-path directory '"
               << appPath << "': " << errorMessage;
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  // create subdirectory js/apps/_db if not yet present
  int res = createBaseApplicationDirectory(appPath, "_db");

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to initialize databases: " << TRI_errno_string(res);
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief activates deadlock detection in all existing databases
void DatabaseFeature::enableDeadlockDetection() {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);

    vocbase->_deadlockDetector.enabled(true);
  }
}

