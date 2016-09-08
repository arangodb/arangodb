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

#include "Agency/v8-agency.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Agency/v8-agency.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Cluster/v8-cluster.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CursorRepository.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/replication-applier.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBIndex.h"
#endif

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

DatabaseFeature* DatabaseFeature::DATABASE = nullptr;

/// @brief database manager thread main loop
/// the purpose of this thread is to physically remove directories of databases
/// that have been dropped
DatabaseManagerThread::DatabaseManagerThread()
    : Thread("DatabaseManager") {}

DatabaseManagerThread::~DatabaseManagerThread() { shutdown(); }

void DatabaseManagerThread::run() {
  auto databaseFeature = ApplicationServer::getFeature<DatabaseFeature>("Database");
  auto dealer = ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  int cleanupCycles = 0;

  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  while (true) {
    // check if we have to drop some database
    TRI_vocbase_t* database = nullptr;

    {
      auto unuser(databaseFeature->_databasesProtector.use());
      auto theLists = databaseFeature->_databasesLists.load();

      for (TRI_vocbase_t* vocbase : theLists->_droppedDatabases) {
        if (!vocbase->canBeDropped()) {
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

        // From now on no other thread can possibly see the old TRI_vocbase_t*,
        // note that there is only one DatabaseManager thread, so it is
        // not possible that another thread has seen this very database
        // and tries to free it at the same time!
      }

      if (database->type() != TRI_VOCBASE_TYPE_COORDINATOR) {
        // regular database
        // ---------------------------

#ifdef ARANGODB_ENABLE_ROCKSDB
        // delete persistent indexes for this database
        RocksDBFeature::dropDatabase(database->id());
#endif

        LOG(TRACE) << "physically removing database directory '"
                    << engine->databasePath(database) << "' of database '" << database->name()
                    << "'";

        std::string path;

        // remove apps directory for database
        auto appPath = dealer->appPath();

        if (database->isOwnAppsDirectory() && !appPath.empty()) {
          path = arangodb::basics::FileUtils::buildFilename(
              arangodb::basics::FileUtils::buildFilename(appPath, "_db"),
              database->name());

          if (TRI_IsDirectory(path.c_str())) {
            LOG(TRACE) << "removing app directory '" << path
                      << "' of database '" << database->name() << "'";

            TRI_RemoveDirectory(path.c_str());
          }
        }

        database->shutdown();
        engine->dropDatabase(database);
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

    // next iteration
  }
}

DatabaseFeature::DatabaseFeature(ApplicationServer* server)
    : ApplicationFeature(server, "Database"),
      _maximalJournalSize(TRI_JOURNAL_DEFAULT_SIZE),
      _defaultWaitForSync(false),
      _forceSyncProperties(true),
      _ignoreDatafileErrors(false),
      _throwCollectionNotLoadedError(false),
      _vocbase(nullptr),
      _databasesLists(new DatabasesLists()),
      _isInitiallyEmpty(false),
      _replicationApplier(true),
      _checkVersion(false),
      _upgrade(false) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");
  startsAfter("EngineSelector");
  startsAfter("LogfileManager");
  startsAfter("InitDatabase");
  startsAfter("IndexPool");
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
}

void DatabaseFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_maximalJournalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG(FATAL) << "invalid value for '--database.maximal-journal-size'. "
                  "expected at least "
               << TRI_JOURNAL_MINIMAL_SIZE;
    FATAL_ERROR_EXIT();
  }

  // sanity check
  if (_checkVersion && _upgrade) {
    LOG(FATAL) << "cannot specify both '--database.check-version' and "
                  "'--database.auto-upgrade'";
    FATAL_ERROR_EXIT();
  }
}

void DatabaseFeature::prepare() {
}

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
    LOG(FATAL) << "could not iterate over all databases: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  if (systemDatabase() == nullptr) {
    LOG(FATAL) << "No _system database found in database directory. Cannot start!";
    FATAL_ERROR_EXIT();
  }

  // start database manager thread
  _databaseManager.reset(new DatabaseManagerThread);

  if (!_databaseManager->start()) {
    LOG(FATAL) << "could not start database manager thread";
    FATAL_ERROR_EXIT();
  }

  // TODO: handle _upgrade and _checkVersion here

  // activate deadlock detection in case we're not running in cluster mode
  if (!arangodb::ServerState::instance()->isRunningInCluster()) {
    enableDeadlockDetection();
  }

  // update all v8 contexts
  updateContexts();
}

void DatabaseFeature::stop() {
  auto logfileManager = arangodb::wal::LogfileManager::instance();
  logfileManager->flush(true, true, false);
  logfileManager->waitForCollector();
}

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
/// this will start the compactors and replication appliers for all databases
int DatabaseFeature::recoveryDone() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);

    // start the compactor for the database
    engine->recoveryDone(vocbase);

    // start the replication applier
    TRI_ASSERT(vocbase->replicationApplier() != nullptr);

    if (vocbase->replicationApplier()->_configuration._autoStart) {
      if (!_replicationApplier) {
        LOG(INFO) << "replication applier explicitly deactivated for database '"
                  << vocbase->name() << "'";
      } else {
        int res = vocbase->replicationApplier()->start(0, false, 0);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "unable to start replication applier for database '"
                    << vocbase->name() << "': " << TRI_errno_string(res);
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief create a new database
int DatabaseFeature::createDatabaseCoordinator(TRI_voc_tick_t id, std::string const& name, TRI_vocbase_t*& result) {
  result = nullptr;

  if (!TRI_vocbase_t::IsAllowedName(true, name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  MUTEX_LOCKER(mutexLocker, _databaseCreateLock);

  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    auto it = theLists->_coordinatorDatabases.find(name);
    if (it != theLists->_coordinatorDatabases.end()) {
      // name already in use
      return TRI_ERROR_ARANGO_DUPLICATE_NAME;
    }
  }

  // name not yet in use, release the read lock
  auto vocbase = std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_COORDINATOR, id, name);

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

  result = vocbase.get();
  vocbase.release();
  return TRI_ERROR_NO_ERROR;
}

/// @brief create a new database
int DatabaseFeature::createDatabase(TRI_voc_tick_t id, std::string const& name,
                                    bool writeMarker, TRI_vocbase_t*& result) {
  result = nullptr;

  if (!TRI_vocbase_t::IsAllowedName(false, name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  if (id == 0) {
    id = TRI_NewTickServer();
  }


  std::unique_ptr<TRI_vocbase_t> vocbase;
  VPackBuilder builder;

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
        return TRI_ERROR_ARANGO_DUPLICATE_NAME;
      }
    }

    builder.openObject();
    builder.add("database", VPackValue(id));
    builder.add("id", VPackValue(std::to_string(id)));
    builder.add("name", VPackValue(name));
    builder.close();

    // create database in storage engine
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    // createDatabase must return a valid database or throw
    vocbase.reset(engine->createDatabase(id, builder.slice()));
    TRI_ASSERT(vocbase != nullptr);

    try {
      vocbase->addReplicationApplier(TRI_CreateReplicationApplier(vocbase.get()));
    } catch (std::exception const& ex) {
      LOG(FATAL) << "initializing replication applier for database '"
                 << vocbase->name() << "' failed: " << ex.what();
      FATAL_ERROR_EXIT();
    }

    // enable deadlock detection
    vocbase->_deadlockDetector.enabled(!arangodb::ServerState::instance()->isRunningInCluster());

    // create application directories
    V8DealerFeature* dealer =
        ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
    auto appPath = dealer->appPath();

    // create app directory for database if it does not exist
    int res = createApplicationDirectory(name, appPath);

    if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
      // starts compactor etc.
      engine->recoveryDone(vocbase.get());

      // start the replication applier
      if (_replicationApplier &&
          vocbase->replicationApplier()->_configuration._autoStart) {
        res = vocbase->replicationApplier()->start(0, false, 0);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "unable to start replication applier for database '"
                    << name << "': " << TRI_errno_string(res);
        }
      }

      // increase reference counter
      vocbase->use();
    }

    {
      MUTEX_LOCKER(mutexLocker, _databasesMutex);
      auto oldLists = _databasesLists.load();
      decltype(oldLists) newLists = nullptr;
      try {
        newLists = new DatabasesLists(*oldLists);
        newLists->_databases.insert(std::make_pair(name, vocbase.get()));
      } catch (...) {
        LOG(ERR) << "Out of memory for putting new database into list!";
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

  if (writeMarker) {
    res = writeCreateMarker(id, builder.slice());
  }

  result = vocbase.release();

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
      LOG(INFO) << "dropping coordinator database '" << vocbase->name() << "'";
      res = TRI_ERROR_NO_ERROR;
    }
  } else {
    delete newLists;
  }
  return res;
}

/// @brief drop database
int DatabaseFeature::dropDatabase(std::string const& name, bool writeMarker, bool waitForDeletion, bool removeAppsDirectory) {
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
        return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
      } else {
        vocbase = it->second;
        id = vocbase->id();
        // mark as deleted
        TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);

        if (!vocbase->markAsDropped()) {
          // deleted by someone else?
          return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
        }

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

    vocbase->setIsOwnAppsDirectory(removeAppsDirectory);

    // invalidate all entries for the database
    arangodb::aql::QueryCache::instance()->invalidate(vocbase);

    res = engine->prepareDropDatabase(vocbase);

    if (res == TRI_ERROR_NO_ERROR) {
      if (writeMarker) {
        // TODO: what shall happen in case writeDropMarker() fails?
        writeDropMarker(id);
      }
    }
  }

  if (res == TRI_ERROR_NO_ERROR && waitForDeletion) {
    engine->waitUntilDeletion(id, true);
  }

  return res;
}

/// @brief drops an existing database
int DatabaseFeature::dropDatabase(TRI_voc_tick_t id, bool writeMarker, bool waitForDeletion, bool removeAppsDirectory) {
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
  return dropDatabase(name, writeMarker, waitForDeletion, removeAppsDirectory);
}

std::vector<TRI_voc_tick_t> DatabaseFeature::getDatabaseIdsCoordinator(bool includeSystem) {
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

std::vector<TRI_voc_tick_t> DatabaseFeature::getDatabaseIds(bool includeSystem) {
  std::vector<TRI_voc_tick_t> ids;

  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
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

      names.emplace_back(vocbase->name());
    }
  }

  std::sort(
      names.begin(), names.end(),
      [](std::string const& l, std::string const& r) -> bool { return l < r; });

  return names;
}

/// @brief return the list of all database names for a user
std::vector<std::string> DatabaseFeature::getDatabaseNamesForUser(std::string const& username) {
  std::vector<std::string> names;

  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);

      auto level =
          GeneralServerFeature::AUTH_INFO.canUseDatabase(username, vocbase->name());

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
  useDatabase(TRI_VOC_SYSTEM_DATABASE);
}

/// @brief get a coordinator database by its id
/// this will increase the reference-counter for the database
TRI_vocbase_t* DatabaseFeature::useDatabaseCoordinator(TRI_voc_tick_t id) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_coordinatorDatabases) {
    TRI_vocbase_t* vocbase = p.second;

    if (vocbase->id() == id) {
      bool result TRI_UNUSED = vocbase->use();

      // if we got here, no one else can have deleted the database
      TRI_ASSERT(result == true);
      return vocbase;
    }
  }
  return nullptr;
}

TRI_vocbase_t* DatabaseFeature::useDatabaseCoordinator(std::string const& name) {
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
    vocbase->use();
    return vocbase;
  }

  return nullptr;
}

TRI_vocbase_t* DatabaseFeature::useDatabase(TRI_voc_tick_t id) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;

    if (vocbase->id() == id) {
      vocbase->use();
      return vocbase;
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

void DatabaseFeature::updateContexts() {
  TRI_ASSERT(_vocbase != nullptr);

  useSystemDatabase();

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  TRI_ASSERT(queryRegistry != nullptr);

  auto vocbase = _vocbase;

  V8DealerFeature* dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  dealer->defineContextUpdate(
      [queryRegistry, vocbase](
          v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t i) {
        TRI_InitV8VocBridge(isolate, context, queryRegistry, vocbase, i);
        TRI_InitV8Queries(isolate, context);
        TRI_InitV8Cluster(isolate, context);
        TRI_InitV8Agency(isolate, context);
      },
      vocbase);
}

void DatabaseFeature::closeDatabases() {
  // stop the replication appliers so all replication transactions can end
  if (_replicationApplier) {
    MUTEX_LOCKER(mutexLocker, _databasesMutex);  // Only one should do this at a time
    // No need for the thread protector here, because we have the mutex

    for (auto& p : _databasesLists.load()->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);
      if (vocbase->replicationApplier() != nullptr) {
        vocbase->replicationApplier()->stop(false);
      }
    }
  }
}

/// @brief close all opened databases
void DatabaseFeature::closeOpenDatabases() {
  MUTEX_LOCKER(mutexLocker, _databasesMutex);  // Only one should do this at a time
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
      LOG(INFO) << "created base application directory '" << path << "'";
    } else {
      if ((res != TRI_ERROR_FILE_EXISTS) || (!TRI_IsDirectory(path.c_str()))) {
        LOG(ERR) << "unable to create base application directory "
                 << errorMessage;
      } else {
        LOG(INFO) << "someone else created base application directory '" << path
                  << "'";
        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  return res;
}

/// @brief create app subdirectory for a database
int DatabaseFeature::createApplicationDirectory(std::string const& name, std::string const& basePath) {
  if (basePath.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  std::string const path = basics::FileUtils::buildFilename(basics::FileUtils::buildFilename(basePath, "_db"), name);
  int res = TRI_ERROR_NO_ERROR;

  if (!TRI_IsDirectory(path.c_str())) {
    long systemError;
    std::string errorMessage;
    res = TRI_CreateRecursiveDirectory(path.c_str(), systemError, errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      if (arangodb::wal::LogfileManager::instance()->isInRecovery()) {
        LOG(TRACE) << "created application directory '" << path
                    << "' for database '" << name << "'";
      } else {
        LOG(INFO) << "created application directory '" << path
                  << "' for database '" << name << "'";
      }
    } else if (res == TRI_ERROR_FILE_EXISTS) {
      LOG(INFO) << "unable to create application directory '" << path
                << "' for database '" << name << "': " << errorMessage;
      res = TRI_ERROR_NO_ERROR;
    } else {
      LOG(ERR) << "unable to create application directory '" << path
                << "' for database '" << name << "': " << errorMessage;
    }
  }

  return res;
}

/// @brief iterate over all databases in the databases directory and open them
int DatabaseFeature::iterateDatabases(VPackSlice const& databases) {
  V8DealerFeature* dealer = ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
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
      TRI_vocbase_t* vocbase = engine->openDatabase(it, _upgrade);
      // we found a valid database
      TRI_ASSERT(vocbase != nullptr);
      
      try {
        vocbase->addReplicationApplier(TRI_CreateReplicationApplier(vocbase));
      } catch (std::exception const& ex) {
        LOG(FATAL) << "initializing replication applier for database '"
                  << vocbase->name() << "' failed: " << ex.what();
        FATAL_ERROR_EXIT();
      }

      if (databaseName == TRI_VOC_SYSTEM_DATABASE) {
        // found the system database
        TRI_ASSERT(_vocbase == nullptr);
        _vocbase = vocbase;
      }

      newLists->_databases.insert(std::make_pair(vocbase->name(), vocbase));
    }
  } catch (std::exception const& ex) {
    delete newLists;

    LOG(FATAL) << "cannot start database: " << ex.what();
    FATAL_ERROR_EXIT();
  } catch (...) {
    delete newLists;

    LOG(FATAL) << "cannot start database: unknown exception";
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
      LOG(ERR) << "unknown database type " << vocbase->type() << " "
               << vocbase->name() << " - close doing nothing.";
    }
  }

  delete oldList;  // Note that this does not delete the TRI_vocbase_t pointers!
}

void DatabaseFeature::verifyAppPaths() {
  // create shared application directory js/apps
  V8DealerFeature* dealer = ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  auto appPath = dealer->appPath();

  if (!appPath.empty() && !TRI_IsDirectory(appPath.c_str())) {
    long systemError;
    std::string errorMessage;
    int res = TRI_CreateRecursiveDirectory(appPath.c_str(), systemError,
                                           errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG(INFO) << "created --javascript.app-path directory '" << appPath << "'";
    } else {
      LOG(ERR) << "unable to create --javascript.app-path directory '"
               << appPath << "': " << errorMessage;
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  // create subdirectory js/apps/_db if not yet present
  int res = createBaseApplicationDirectory(appPath, "_db");

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "unable to initialize databases: " << TRI_errno_string(res);
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

/// @brief writes a create-database marker into the log
int DatabaseFeature::writeCreateMarker(TRI_voc_tick_t id, VPackSlice const& slice) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::DatabaseMarker marker(TRI_DF_MARKER_VPACK_CREATE_DATABASE,
                                         id, slice);
    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      // throw an exception which is caught at the end of this function
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "could not save create database marker in log: "
              << TRI_errno_string(res);
  }

  return res;
}

/// @brief writes a drop-database marker into the log
int DatabaseFeature::writeDropMarker(TRI_voc_tick_t id) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("id", VPackValue(std::to_string(id)));
    builder.close();

    arangodb::wal::DatabaseMarker marker(TRI_DF_MARKER_VPACK_DROP_DATABASE, id,
                                         builder.slice());

    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      // throw an exception which is caught at the end of this function
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "could not save drop database marker in log: "
              << TRI_errno_string(res);
  }

  return res;
}
