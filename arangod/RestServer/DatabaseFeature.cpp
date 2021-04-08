////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "DatabaseFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationClients.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/CursorRepository.h"
#include "Utils/Events.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
arangodb::CreateDatabaseInfo createExpressionVocbaseInfo(arangodb::application_features::ApplicationServer& server) {
  arangodb::CreateDatabaseInfo info(server, arangodb::ExecContext::current());
  auto rv = info.load("Z", std::numeric_limits<uint64_t>::max()); // name does not matter. We just need validity check to pass.
  TRI_ASSERT(rv.ok());
  return info;
}

/// @brief sandbox vocbase for executing calculation queries
std::unique_ptr<TRI_vocbase_t> calculationVocbase;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // i am here for debugging only.
TRI_vocbase_t* DatabaseFeature::CURRENT_VOCBASE = nullptr;
#endif

/// @brief database manager thread main loop
/// the purpose of this thread is to physically remove directories of databases
/// that have been dropped
DatabaseManagerThread::DatabaseManagerThread(ApplicationServer& server)
    : Thread(server, "DatabaseManager") {}

DatabaseManagerThread::~DatabaseManagerThread() { shutdown(); }

void DatabaseManagerThread::run() {
  auto& databaseFeature = server().getFeature<DatabaseFeature>();
  auto& dealer = server().getFeature<V8DealerFeature>();
  int cleanupCycles = 0;

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  while (true) {
    try {
      // check if we have to drop some database
      TRI_vocbase_t* database = nullptr;

      {
        auto unuser(databaseFeature._databasesProtector.use());
        auto theLists = databaseFeature._databasesLists.load();

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
          MUTEX_LOCKER(mutexLocker, databaseFeature._databasesMutex);

          // Build the new value:
          auto oldLists = databaseFeature._databasesLists.load();
          decltype(oldLists) newLists = nullptr;
          try {
            newLists = new DatabaseFeature::DatabasesLists();
            newLists->_databases = oldLists->_databases;
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
          databaseFeature._databasesLists = newLists;
          databaseFeature._databasesProtector.scan();
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

          {
            // remove apps directory for database
            std::string const& appPath = dealer.appPath();
            if (database->isOwnAppsDirectory() && !appPath.empty()) {
              MUTEX_LOCKER(mutexLocker1, databaseFeature._databaseCreateLock);

              // but only if nobody re-created a database with the same name!
              MUTEX_LOCKER(mutexLocker2, databaseFeature._databasesMutex);
              
              TRI_vocbase_t* newInstance = databaseFeature.lookupDatabase(database->name());
              TRI_ASSERT(newInstance == nullptr || newInstance->id() != database->id());

              if (newInstance == nullptr) {
                std::string path = arangodb::basics::FileUtils::buildFilename(
                    arangodb::basics::FileUtils::buildFilename(appPath, "_db"),
                    database->name());
  
                if (TRI_IsDirectory(path.c_str())) {
                  LOG_TOPIC("041b1", TRACE, arangodb::Logger::FIXME)
                    << "removing app directory '" << path << "' of database '"
                    << database->name() << "'";

                  TRI_RemoveDirectory(path.c_str());
                }
              }
            }
          }

          // destroy all items in the QueryRegistry for this database
          auto queryRegistry = QueryRegistryFeature::registry();
          if (queryRegistry != nullptr) {
            // but only if nobody re-created a database with the same name!
            MUTEX_LOCKER(mutexLocker, databaseFeature._databasesMutex);
            TRI_vocbase_t* newInstance = databaseFeature.lookupDatabase(database->name());
            TRI_ASSERT(newInstance == nullptr || newInstance->id() != database->id());

            if (newInstance == nullptr) {
              queryRegistry->destroy(database->name());
            }
          }

          try {
            Result res = engine.dropDatabase(*database);
            if (res.fail()) {
              LOG_TOPIC("fb244", ERR, Logger::FIXME)
                << "dropping database '" << database->name() << "' failed: " << res.errorMessage();
            }
          } catch (std::exception const& ex) {
            LOG_TOPIC("d30a2", ERR, Logger::FIXME) << "dropping database '" << database->name()
                                          << "' failed: " << ex.what();
          } catch (...) {
            LOG_TOPIC("0a30c", ERR, Logger::FIXME)
                << "dropping database '" << database->name() << "' failed";
          }
        }

        delete database;

        // directly start next iteration
      } else {  // if (database != nullptr)
        // perfom some cleanup tasks
        if (isStopping()) {
          // done
          break;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(waitTime()));

        // The following is only necessary after a wait:
        auto queryRegistry = QueryRegistryFeature::registry();
        if (queryRegistry != nullptr) {
          queryRegistry->expireQueries();
        }

        // perform cursor cleanup here
        if (++cleanupCycles >= 10) {
          cleanupCycles = 0;

          auto unuser(databaseFeature._databasesProtector.use());
          auto theLists = databaseFeature._databasesLists.load();

          bool force = isStopping();
          for (auto& p : theLists->_databases) {
            TRI_vocbase_t* vocbase = p.second;
            TRI_ASSERT(vocbase != nullptr);

            try {
              vocbase->cursorRepository()->garbageCollect(force);
            } catch (...) {
            }
            double const now = []() {
              using namespace std::chrono;
              return duration<double>(steady_clock::now().time_since_epoch()).count();
            }();
            vocbase->replicationClients().garbageCollect(now);
          }
        }
      }

    } catch (...) {
    }

    // next iteration
  }
}

DatabaseFeature::DatabaseFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Database"),
      _defaultWaitForSync(false),
      _forceSyncProperties(true),
      _ignoreDatafileErrors(false),
      _databasesLists(new DatabasesLists()),
      _isInitiallyEmpty(false),
      _checkVersion(false),
      _upgrade(false),
      _started(false) {
  setOptional(false);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<AuthenticationFeature>();
  startsAfter<CacheManagerFeature>();
  startsAfter<EngineSelectorFeature>();
  startsAfter<InitDatabaseFeature>();
  startsAfter<StorageEngineFeature>();
}

DatabaseFeature::~DatabaseFeature() {
  // clean up
  auto p = _databasesLists.load();
  delete p;
}

void DatabaseFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addOption("--database.wait-for-sync",
                     "default wait-for-sync behavior, can be overwritten "
                     "when creating a collection",
                     new BooleanParameter(&_defaultWaitForSync),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--database.force-sync-properties",
                     "force syncing of collection properties to disk, "
                     "will use waitForSync value of collection when "
                     "turned off",
                     new BooleanParameter(&_forceSyncProperties),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--database.ignore-datafile-errors",
                     "load collections even if datafiles may contain errors",
                     new BooleanParameter(&_ignoreDatafileErrors),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  // the following option was obsoleted in 3.9
  options->addObsoleteOption(
      "--database.old-system-collections",
      "create and use deprecated system collection (_modules, _fishbowl)", false);
  
  // the following option was obsoleted in 3.8
  options->addObsoleteOption(
      "--database.throw-collection-not-loaded-error",
      "throw an error when accessing a collection that is still loading", false);
  
  // the following option was removed in 3.7
  options->addObsoleteOption("--database.maximal-journal-size",
                             "default maximal journal size, can be overwritten when "
                             "creating a collection", true);


  // the following option was removed in 3.2
  options->addObsoleteOption(
      "--database.index-threads",
      "threads to start for parallel background index creation", true);

  // the following hidden option was removed in 3.4
  options->addObsoleteOption(
      "--database.check-30-revisions",
      "check for revision values from ArangoDB 3.0 databases", true);

  // the following options were removed in 3.2
  options->addObsoleteOption(
      "--database.revision-cache-chunk-size",
      "chunk size (in bytes) for the document revisions cache", true);
  options->addObsoleteOption(
      "--database.revision-cache-target-size",
      "total target size (in bytes) for the document revisions cache", true);
}

void DatabaseFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check the misuse of startup options
  if (_checkVersion && _upgrade) {
    LOG_TOPIC("a25b0", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both '--database.check-version' and "
           "'--database.auto-upgrade'";
    FATAL_ERROR_EXIT();
  }
}

void DatabaseFeature::initCalculationVocbase(application_features::ApplicationServer& server) {
  calculationVocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL,
                                      createExpressionVocbaseInfo(server));
}

void DatabaseFeature::start() {
  verifyAppPaths();

  // scan all databases
  VPackBuilder builder;
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.getDatabases(builder);

  TRI_ASSERT(builder.slice().isArray());

  auto res = iterateDatabases(builder.slice());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("0c49d", FATAL, arangodb::Logger::FIXME)
        << "could not iterate over all databases: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  if (!lookupDatabase(StaticStrings::SystemDatabase)) {
    LOG_TOPIC("97e7c", FATAL, arangodb::Logger::FIXME)
        << "No _system database found in database directory. Cannot start!";
    FATAL_ERROR_EXIT();
  }

  // start database manager thread
  _databaseManager.reset(new DatabaseManagerThread(server()));

  if (!_databaseManager->start()) {
    LOG_TOPIC("7eb06", FATAL, arangodb::Logger::FIXME)
        << "could not start database manager thread";
    FATAL_ERROR_EXIT();
  }

  // activate deadlock detection in case we're not running in cluster mode
  if (!arangodb::ServerState::instance()->isRunningInCluster()) {
    enableDeadlockDetection();
  }

  _started.store(true);
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

    // throw away all open cursors in order to speed up shutdown
    vocbase->cursorRepository()->garbageCollect(true);
  }
}

void DatabaseFeature::stop() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // i am here for debugging only.
  static TRI_vocbase_t* currentVocbase = nullptr;
#endif

  stopAppliers();

  // turn off query cache and flush it
  arangodb::aql::QueryCacheProperties p;
  p.mode = arangodb::aql::QueryCacheMode::CACHE_ALWAYS_OFF;
  p.maxResultsCount = 0;
  p.maxResultsSize = 0;
  p.maxEntrySize = 0;
  p.includeSystem = false;
  p.showBindVars = false;

  arangodb::aql::QueryCache::instance()->properties(p);
  arangodb::aql::QueryCache::instance()->invalidate();

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.cleanupReplicationContexts();

  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto queryRegistry = QueryRegistryFeature::registry();
  if (queryRegistry != nullptr) {
    TRI_ASSERT(queryRegistry->numberRegisteredQueries() == 0);
  }
#endif

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;

    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
    if (vocbase->type() != TRI_VOCBASE_TYPE_NORMAL) {
      continue;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // i am here for debugging only.
    currentVocbase = vocbase;
    CURRENT_VOCBASE = vocbase;
    static size_t currentCursorCount = currentVocbase->cursorRepository()->count();
    static size_t currentQueriesCount = currentVocbase->queryList()->count();

    LOG_TOPIC("840a4", DEBUG, Logger::FIXME)
        << "shutting down database " << currentVocbase->name() << ": " << (void*) currentVocbase
        << ", cursors: " << currentCursorCount
        << ", queries: " << currentQueriesCount;
#endif
    vocbase->stop();

    vocbase->processCollections(
        [](LogicalCollection* collection) {
          // no one else must modify the collection's status while we are in
          // here
          collection->executeWhileStatusWriteLocked(
              [collection]() { collection->close(); });
        },
        true);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // i am here for debugging only.
    LOG_TOPIC("4b2b7", DEBUG, Logger::FIXME)
        << "shutting down database " << currentVocbase->name() << ": " << (void*) currentVocbase << " successful";
#endif
  }

  // flush again so we are sure no query is left in the cache here
  arangodb::aql::QueryCache::instance()->invalidate();
}

void DatabaseFeature::unprepare() {
  // delete the database manager thread
  if (_databaseManager != nullptr) {
    _databaseManager->beginShutdown();

    while (_databaseManager->isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  try {
    closeDroppedDatabases();
  } catch (...) {
    // we're in the shutdown... simply ignore any errors produced here
  }

  _databaseManager.reset();

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // This is to avoid heap use after free errors in the iresearch tests, because
  // the destruction a callback uses a database.
  // I don't know if this is safe to do, thus I enclosed it in ARANGODB_USE_GOOGLE_TESTS
  // to prevent accidentally breaking anything. However,
  // TODO Find out if this is okay and may be merged (maybe without the #ifdef),
  // or if this has to be done differently in the tests instead. The errors may
  // also go away when some new PR is merged, so maybe this can just be removed
  // in the future.
  _pendingRecoveryCallbacks.clear();
#endif

  try {
    // closeOpenDatabases() can throw, but we're in a dtor
    closeOpenDatabases();
  } catch (...) {
  }
  calculationVocbase.reset();
}

void DatabaseFeature::prepare() {
  // need this to make calculation analyzer available in database links
  initCalculationVocbase(server());
}

/// @brief will be called when the recovery phase has run
/// this will call the engine-specific recoveryDone() procedures
/// and will execute engine-unspecific operations (such as starting
/// the replication appliers) for all databases
void DatabaseFeature::recoveryDone() {
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  TRI_ASSERT(!engine.inRecovery());

  // '_pendingRecoveryCallbacks' will not change because
  // !StorageEngine.inRecovery()
  for (auto& entry : _pendingRecoveryCallbacks) {
    auto result = entry();

    if (!result.ok()) {
      LOG_TOPIC("772a7", ERR, arangodb::Logger::FIXME)
          << "recovery failure due to error from callback, error '"
          << TRI_errno_string(result.errorNumber())
          << "' message: " << result.errorMessage();

      THROW_ARANGO_EXCEPTION(result);
    }
  }

  _pendingRecoveryCallbacks.clear();

  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
    if (vocbase->type() != TRI_VOCBASE_TYPE_NORMAL) {
      continue;
    }

    if (vocbase->replicationApplier()) {
      if (server().hasFeature<ReplicationFeature>()) {
        server().getFeature<ReplicationFeature>().startApplier(vocbase);
      }
    }
  }
}

Result DatabaseFeature::registerPostRecoveryCallback(std::function<Result()>&& callback) {
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  if (!engine.inRecovery()) {
    return callback();  // if no engine then can't be in recovery
  }

  // do not need a lock since single-thread access during recovery
  _pendingRecoveryCallbacks.emplace_back(std::move(callback));

  return Result();
}

bool DatabaseFeature::started() const noexcept {
  return _started.load(std::memory_order_relaxed);
}
  
void DatabaseFeature::enumerate(std::function<void(TRI_vocbase_t*)> const& callback) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    callback(p.second);
  }
}

/// @brief create a new database
Result DatabaseFeature::createDatabase(CreateDatabaseInfo&& info, TRI_vocbase_t*& result) {
  std::string name = info.getName();
  auto dbId = info.getId();
  VPackBuilder markerBuilder;
  {
    VPackObjectBuilder guard(&markerBuilder);
    info.toVelocyPack(markerBuilder); // can we improve this
  }
  result = nullptr;

  if (!TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(name))) {
    return {TRI_ERROR_ARANGO_DATABASE_NAME_INVALID};
  }

  std::unique_ptr<TRI_vocbase_t> vocbase;

  // create database in storage engine
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  // the create lock makes sure no one else is creating a database while we're
  // inside this function
  MUTEX_LOCKER(mutexLocker, _databaseCreateLock);
  {
    {
      auto unuser(_databasesProtector.use());
      auto theLists = _databasesLists.load();

      auto it = theLists->_databases.find(name);
      if (it != theLists->_databases.end()) {
        // name already in use
        return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("duplicate database name '") + name + "'");
      }
    }

    // createDatabase must return a valid database or throw
    auto status = TRI_ERROR_NO_ERROR;

    vocbase = engine.createDatabase(std::move(info), status);
    TRI_ASSERT(status == TRI_ERROR_NO_ERROR);
    TRI_ASSERT(vocbase != nullptr);

    if (vocbase->type() == TRI_VOCBASE_TYPE_NORMAL) {
      try {
        vocbase->addReplicationApplier();
      } catch (basics::Exception const& ex) {
        std::string msg = "initializing replication applier for database '" +
            vocbase->name() + "' failed: " + ex.what();
        LOG_TOPIC("e7444", ERR, arangodb::Logger::FIXME) << msg;
        return Result(ex.code(), std::move(msg));
      } catch (std::exception const& ex) {
        std::string msg = "initializing replication applier for database '" +
            vocbase->name() + "' failed: " + ex.what();
        LOG_TOPIC("56c41", ERR, arangodb::Logger::FIXME) << msg;
        return Result(TRI_ERROR_INTERNAL, std::move(msg));
      }

      // enable deadlock detection
      vocbase->_deadlockDetector.enabled(!ServerState::instance()->isRunningInCluster());

      // create application directories
      V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
      auto appPath = dealer.appPath();

      // create app directory for database if it does not exist
      auto res = createApplicationDirectory(name, appPath, true);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    if (!engine.inRecovery()) {
      if (vocbase->type() == TRI_VOCBASE_TYPE_NORMAL) {
        if (server().hasFeature<ReplicationFeature>()) {
          server().getFeature<ReplicationFeature>().startApplier(vocbase.get());
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
        LOG_TOPIC("34825", ERR, arangodb::Logger::FIXME)
            << "Out of memory for putting new database into list!";
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
  Result res;

  if (!engine.inRecovery()) {
    res = engine.writeCreateDatabaseMarker(dbId, markerBuilder.slice());
  }

  result = vocbase.release();

  if (versionTracker() != nullptr) {
    versionTracker()->track("create database");
  }

  return res;
}

/// @brief drop database
ErrorCode DatabaseFeature::dropDatabase(std::string const& name, bool removeAppsDirectory) {
  if (name == StaticStrings::SystemDatabase) {
    // prevent deletion of system database
    return TRI_ERROR_FORBIDDEN;
  }

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  TRI_voc_tick_t id = 0;
  auto res = TRI_ERROR_NO_ERROR;
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
      }

      vocbase = it->second;
      id = vocbase->id();
      // mark as deleted

      // call LogicalDataSource::drop() to allow instances to clean up internal
      // state (e.g. for LogicalView implementations)
      TRI_vocbase_t::dataSourceVisitor visitor =
          [&res, &vocbase](arangodb::LogicalDataSource& dataSource) -> bool {
        // skip LogicalCollection since their internal state is always in the
        // StorageEngine (optimization)
        if (arangodb::LogicalCollection::category() == dataSource.category()) {
          return true;
        }

        auto result = dataSource.drop();

        if (!result.ok()) {
          res = result.errorNumber();
          LOG_TOPIC("c44cb", ERR, arangodb::Logger::FIXME)
              << "failed to drop DataSource '" << dataSource.name()
              << "' while dropping database '" << vocbase->name()
              << "': " << result.errorNumber() << " " << result.errorMessage();
        }

        return true;  // try next DataSource
      };

      vocbase->visitDataSources(visitor, true);  // acquire a write lock to avoid potential deadlocks

      if (TRI_ERROR_NO_ERROR != res) {
        return res;
      }

      newLists->_databases.erase(it);
      newLists->_droppedDatabases.insert(vocbase);
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

    if (server().hasFeature<arangodb::iresearch::IResearchAnalyzerFeature>()) {
      server().getFeature<arangodb::iresearch::IResearchAnalyzerFeature>().invalidate(*vocbase);
    }
          
    auto queryRegistry = QueryRegistryFeature::registry();
    if (queryRegistry != nullptr) {
      queryRegistry->destroy(vocbase->name());
    }

    res = engine.prepareDropDatabase(*vocbase).errorNumber();
  }
  // must not use the database after here, as it may now be
  // deleted by the DatabaseManagerThread!

  if (versionTracker() != nullptr) {
    versionTracker()->track("drop database");
  }

  return res;
}

/// @brief drops an existing database
ErrorCode DatabaseFeature::dropDatabase(TRI_voc_tick_t id, bool removeAppsDirectory) {
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

  if (name.empty()) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }
  // and call the regular drop function
  return dropDatabase(name, removeAppsDirectory);
}

std::vector<TRI_voc_tick_t> DatabaseFeature::getDatabaseIds(bool includeSystem) {
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
      if (includeSystem || vocbase->name() != StaticStrings::SystemDatabase) {
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

  std::sort(names.begin(), names.end(), [](std::string const& l, std::string const& r) -> bool {
    return l < r;
  });

  return names;
}

/// @brief return the list of all database names for a user
std::vector<std::string> DatabaseFeature::getDatabaseNamesForUser(std::string const& username) {
  std::vector<std::string> names;

  AuthenticationFeature* af = AuthenticationFeature::instance();
  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase->isDropped()) {
        continue;
      }

      if (af->isActive() && af->userManager() != nullptr) {
        auto level = af->userManager()->databaseAuthLevel(username, vocbase->name());
        if (level == auth::Level::NONE) {  // hide dbs without access
          continue;
        }
      }

      names.emplace_back(vocbase->name());
    }
  }

  std::sort(names.begin(), names.end(), [](std::string const& l, std::string const& r) -> bool {
    return l < r;
  });

  return names;
}

/// @brief return the list of all database names
void DatabaseFeature::inventory(VPackBuilder& result, TRI_voc_tick_t maxTick,
                                std::function<bool(arangodb::LogicalCollection const*)> const& nameFilter) {
  result.openObject();
  {
    auto unuser(_databasesProtector.use());
    auto theLists = _databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase->isDropped()) {
        continue;
      }

      result.add(vocbase->name(), VPackValue(VPackValueType::Object));
      result.add("id", VPackValue(std::to_string(vocbase->id())));
      result.add("name", VPackValue(vocbase->name()));
      vocbase->inventory(result, maxTick, nameFilter);
      result.close();
    }
  }
  result.close();
}

TRI_vocbase_t* DatabaseFeature::useDatabase(std::string const& name) const {
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

TRI_vocbase_t* DatabaseFeature::useDatabase(TRI_voc_tick_t id) const {
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
TRI_vocbase_t* DatabaseFeature::lookupDatabase(std::string const& name) const {
  if (name.empty()) {
    return nullptr;
  }

  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  // database names with a number in front are invalid names
  if (name[0] >= '0' && name[0] <= '9') {
    TRI_voc_tick_t id =
        NumberUtils::atoi_zero<TRI_voc_tick_t>(name.data(), name.data() + name.size());
    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      if (vocbase->id() == id) {
        return vocbase;
      }
    }
  } else {
    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      if (name == vocbase->name()) {
        return vocbase;
      }
    }
  }

  return nullptr;
}

std::string DatabaseFeature::translateCollectionName(std::string const& dbName,
                                                     std::string const& collectionName) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();
  auto itr = theLists->_databases.find(dbName);

  if (itr == theLists->_databases.end()) {
    return std::string();
  }

  auto* vocbase = itr->second;
  TRI_ASSERT(vocbase != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR);
    CollectionNameResolver resolver(*vocbase);

    return resolver.getCollectionNameCluster(
        DataSourceId{NumberUtils::atoi_zero<DataSourceId::BaseType>(
            collectionName.data(), collectionName.data() + collectionName.size())});
  } else {
    TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);
    auto collection = vocbase->lookupCollection(collectionName);

    return collection ? collection->name() : std::string();
  }
}

void DatabaseFeature::enumerateDatabases(std::function<void(TRI_vocbase_t& vocbase)> const& func) {
  auto unuser(_databasesProtector.use());
  auto theLists = _databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
    func(*vocbase);
  }
}

TRI_vocbase_t& arangodb::DatabaseFeature::getCalculationVocbase() {
  TRI_ASSERT(calculationVocbase);
  return *calculationVocbase;
}

void DatabaseFeature::stopAppliers() {
  // stop the replication appliers so all replication transactions can end
  if (!server().hasFeature<ReplicationFeature>()) {
    return;
  }

  ReplicationFeature& replicationFeature = server().getFeature<ReplicationFeature>();

  MUTEX_LOCKER(mutexLocker,
               _databasesMutex);  // Only one should do this at a time
  // No need for the thread protector here, because we have the mutex

  for (auto& p : _databasesLists.load()->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    if (vocbase->type() == TRI_VOCBASE_TYPE_NORMAL) {
      replicationFeature.stopApplier(vocbase);
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
    vocbase->shutdown();

    delete vocbase;
  }

  delete oldList;  // Note that this does not delete the TRI_vocbase_t pointers!
}

/// @brief create base app directory
ErrorCode DatabaseFeature::createBaseApplicationDirectory(std::string const& appPath,
                                                          std::string const& type) {
  auto res = TRI_ERROR_NO_ERROR;
  std::string path = arangodb::basics::FileUtils::buildFilename(appPath, type);

  if (!TRI_IsDirectory(path.c_str())) {
    std::string errorMessage;
    long systemError;
    res = TRI_CreateDirectory(path.c_str(), systemError, errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("e6460", INFO, arangodb::Logger::FIXME)
          << "created base application directory '" << path << "'";
    } else {
      if ((res != TRI_ERROR_FILE_EXISTS) || (!TRI_IsDirectory(path.c_str()))) {
        LOG_TOPIC("5a0b4", ERR, arangodb::Logger::FIXME)
            << "unable to create base application directory " << errorMessage;
      } else {
        LOG_TOPIC("0a25f", INFO, arangodb::Logger::FIXME)
            << "someone else created base application directory '" << path << "'";
        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  return res;
}

/// @brief create app subdirectory for a database
ErrorCode DatabaseFeature::createApplicationDirectory(std::string const& name,
                                                      std::string const& basePath,
                                                      bool removeExisting) {
  if (basePath.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  std::string const path = basics::FileUtils::buildFilename(
      basics::FileUtils::buildFilename(basePath, "_db"), name);

  if (TRI_IsDirectory(path.c_str())) {
    // directory already exists
    // this can happen if a database is dropped and quickly recreated
    if (!removeExisting) {
      return TRI_ERROR_NO_ERROR;
    }

    if (!basics::FileUtils::listFiles(path).empty()) {
      LOG_TOPIC("56fc7", INFO, arangodb::Logger::FIXME)
          << "forcefully removing existing application directory '" << path
          << "' for database '" << name << "'";
      // removing is best effort. if it does not succeed, we can still
      // go on creating the it
      TRI_RemoveDirectory(path.c_str());
    }
  }

  // directory does not yet exist - this should be the standard case
  long systemError;
  std::string errorMessage;
  auto res = TRI_CreateRecursiveDirectory(path.c_str(), systemError, errorMessage);

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("6745a", TRACE, arangodb::Logger::FIXME)
        << "created application directory '" << path << "' for database '"
        << name << "'";
  } else if (res == TRI_ERROR_FILE_EXISTS) {
    LOG_TOPIC("2a78e", INFO, arangodb::Logger::FIXME)
        << "unable to create application directory '" << path
        << "' for database '" << name << "': " << errorMessage;
    res = TRI_ERROR_NO_ERROR;
  } else {
    LOG_TOPIC("36682", ERR, arangodb::Logger::FIXME)
        << "unable to create application directory '" << path
        << "' for database '" << name << "': " << errorMessage;
  }

  return res;
}

/// @brief iterate over all databases in the databases directory and open them
ErrorCode DatabaseFeature::iterateDatabases(VPackSlice const& databases) {
  V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
  std::string const appPath = dealer.appPath();

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  auto res = TRI_ERROR_NO_ERROR;

  // open databases in defined order
  MUTEX_LOCKER(mutexLocker, _databasesMutex);

  auto oldLists = _databasesLists.load();
  auto newLists = new DatabasesLists(*oldLists);

  ServerState::RoleEnum role = arangodb::ServerState::instance()->getRole();

  try {
    for (VPackSlice it : VPackArrayIterator(databases)) {
      TRI_ASSERT(it.isObject());

      LOG_TOPIC("95f68", TRACE, Logger::FIXME) << "processing database: " << it.toJson();

      VPackSlice deleted = it.get("deleted");
      if (deleted.isBoolean() && deleted.getBoolean()) {
        // ignore deleted databases here
        continue;
      }

      std::string const databaseName = it.get("name").copyString();

      // create app directory for database if it does not exist
      res = createApplicationDirectory(databaseName, appPath, false);

      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }

      // open the database and scan collections in it

      // try to open this database
      arangodb::CreateDatabaseInfo info(server(), ExecContext::current());
      auto res = info.load(it, VPackSlice::emptyArraySlice());
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      auto database = engine.openDatabase(std::move(info), _upgrade);

      if (!ServerState::isCoordinator(role) && !ServerState::isAgent(role)) {
        try {
          database->addReplicationApplier();
        } catch (std::exception const& ex) {
          LOG_TOPIC("ff848", FATAL, arangodb::Logger::FIXME)
              << "initializing replication applier for database '"
              << database->name() << "' failed: " << ex.what();
          FATAL_ERROR_EXIT();
        }
      }

      newLists->_databases.insert(std::make_pair(database->name(), database.get()));
      database.release();
    }
  } catch (std::exception const& ex) {
    delete newLists;

    LOG_TOPIC("c7dc0", FATAL, arangodb::Logger::FIXME) << "cannot start database: " << ex.what();
    FATAL_ERROR_EXIT();
  } catch (...) {
    delete newLists;

    LOG_TOPIC("79053", FATAL, arangodb::Logger::FIXME)
        << "cannot start database: unknown exception";
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
      LOG_TOPIC("b8b0e", ERR, arangodb::Logger::FIXME)
          << "unknown database type " << vocbase->type() << " "
          << vocbase->name() << " - close doing nothing.";
    }
  }

  delete oldList;  // Note that this does not delete the TRI_vocbase_t pointers!
}

void DatabaseFeature::verifyAppPaths() {
  // create shared application directory js/apps
  V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
  auto appPath = dealer.appPath();

  if (!appPath.empty() && !TRI_IsDirectory(appPath.c_str())) {
    long systemError;
    std::string errorMessage;
    auto res = TRI_CreateRecursiveDirectory(appPath.c_str(), systemError, errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("1bf74", INFO, arangodb::Logger::FIXME)
          << "created --javascript.app-path directory '" << appPath << "'";
    } else {
      LOG_TOPIC("52bd5", ERR, arangodb::Logger::FIXME)
          << "unable to create --javascript.app-path directory '" << appPath
          << "': " << errorMessage;
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  // create subdirectory js/apps/_db if not yet present
  auto res = createBaseApplicationDirectory(appPath, "_db");

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("610c7", ERR, arangodb::Logger::FIXME)
        << "unable to initialize databases: " << TRI_errno_string(res);
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
