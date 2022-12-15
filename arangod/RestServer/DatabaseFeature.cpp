////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationClients.h"
#include "Replication/ReplicationFeature.h"
#include "Replication2/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utilities/NameValidator.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/CursorRepository.h"
#include "Utils/Events.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {

template<typename T>
void waitUnique(std::shared_ptr<T> const& ptr) {
  // It's safe because we guarantee we have not weak_ptr
  while (ptr.use_count() != 1) {
    // 250ms from old DataProtector code authored by Max Neunhoeffer
    std::this_thread::sleep_for(std::chrono::microseconds(250));
  }
  std::atomic_thread_fence(std::memory_order_acquire);
}

arangodb::CreateDatabaseInfo createExpressionVocbaseInfo(
    arangodb::ArangodServer& server) {
  arangodb::CreateDatabaseInfo info(server, arangodb::ExecContext::current());
  auto rv = info.load(
      "Z",
      std::numeric_limits<uint64_t>::max());  // name does not matter. We just
                                              // need validity check to pass.
  TRI_ASSERT(rv.ok());
  return info;
}

/// @brief return either the name of the database to be used as a folder name,
/// or its id if its name contains special characters and is not fully supported
/// in every OS
[[nodiscard]] std::string getDatabaseDirName(std::string const& databaseName,
                                             std::string const& id) {
  bool isOldStyleName = DatabaseNameValidator::isAllowedName(
      /*allowSystem*/ true, /*extendedNames*/ false, databaseName);
  return (isOldStyleName || id.empty()) ? databaseName : id;
}

/// @brief sandbox vocbase for executing calculation queries
std::unique_ptr<TRI_vocbase_t> calculationVocbase;
}  // namespace

/// @brief database manager thread main loop
/// the purpose of this thread is to physically remove directories of databases
/// that have been dropped
DatabaseManagerThread::DatabaseManagerThread(Server& server)
    : ServerThread<ArangodServer>(server, "DatabaseManager") {}

DatabaseManagerThread::~DatabaseManagerThread() { shutdown(); }

void DatabaseManagerThread::run() {
  auto& databaseFeature = server().getFeature<DatabaseFeature>();
  auto& dealer = server().getFeature<V8DealerFeature>();
  int cleanupCycles = 0;

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  while (true) {
    try {
      // if we find a database to delete, it will be automatically deleted at
      // the end of this scope
      std::unique_ptr<TRI_vocbase_t> database;
      {
        std::lock_guard lock{databaseFeature._databasesMutex};
        auto& dropped = databaseFeature._droppedDatabases;
        // check if we have to drop some database
        for (auto it = dropped.begin(), end = dropped.end(); it != end; ++it) {
          TRI_ASSERT(*it != nullptr);
          if ((*it)->isDangling()) {
            database.reset(*it);
            dropped.erase(it);
            break;  // found a database to delete
          }
        }
      }

      if (database != nullptr) {
        if (!ServerState::instance()->isCoordinator()) {
          TRI_ASSERT(!database->isSystem());

          if (dealer.isEnabled()) {
            // remove apps directory for database
            std::string const& appPath = dealer.appPath();
            if (database->isOwnAppsDirectory() && !appPath.empty()) {
              std::lock_guard lockCreate{databaseFeature._databaseCreateLock};

              // but only if nobody re-created a database with the same name!
              std::lock_guard lock{databaseFeature._databasesMutex};

              TRI_vocbase_t* newInstance =
                  databaseFeature.lookupDatabase(database->name());
              TRI_ASSERT(newInstance == nullptr ||
                         newInstance->id() != database->id());
              if (newInstance == nullptr) {
                std::string const dirName = ::getDatabaseDirName(
                    database->name(), std::to_string(database->id()));
                std::string path = arangodb::basics::FileUtils::buildFilename(
                    arangodb::basics::FileUtils::buildFilename(appPath, "_db"),
                    dirName);

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
            std::lock_guard lock{databaseFeature._databasesMutex};
            TRI_vocbase_t* newInstance =
                databaseFeature.lookupDatabase(database->name());
            TRI_ASSERT(newInstance == nullptr ||
                       newInstance->id() != database->id());

            if (newInstance == nullptr) {
              queryRegistry->destroy(database->name());
            }
          }

          try {
            Result res = engine.dropDatabase(*database);
            if (res.fail()) {
              LOG_TOPIC("fb244", ERR, Logger::FIXME)
                  << "dropping database '" << database->name()
                  << "' failed: " << res.errorMessage();
            }
          } catch (std::exception const& ex) {
            LOG_TOPIC("d30a2", ERR, Logger::FIXME)
                << "dropping database '" << database->name()
                << "' failed: " << ex.what();
          } catch (...) {
            LOG_TOPIC("0a30c", ERR, Logger::FIXME)
                << "dropping database '" << database->name() << "' failed";
          }
        }

        // directly start next iteration
      } else {  // if (database != nullptr)
        // perform some cleanup tasks
        if (isStopping()) {
          // done
          break;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(waitTime()));

        // The following is only necessary after a wait:
        if (!ServerState::instance()->isSingleServer()) {
          auto queryRegistry = QueryRegistryFeature::registry();
          if (queryRegistry != nullptr) {
            queryRegistry->expireQueries();
          }
        }

        // perform cursor cleanup here
        if (++cleanupCycles >= 10) {
          cleanupCycles = 0;

          auto databases = databaseFeature._databases.load();

          bool force = isStopping();
          for (auto& p : *databases) {
            TRI_vocbase_t* vocbase = p.second;
            TRI_ASSERT(vocbase != nullptr);

            try {
              vocbase->cursorRepository()->garbageCollect(force);
            } catch (...) {
            }
            double const now = []() {
              using namespace std::chrono;
              return duration<double>(steady_clock::now().time_since_epoch())
                  .count();
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

struct HeartbeatTimescale {
  static arangodb::metrics::LogScale<double> scale() {
    return {10.0, 0.0, 1000000.0, 8};
  }
};

DECLARE_HISTOGRAM(arangodb_ioheartbeat_duration, HeartbeatTimescale,
                  "Time to execute the io heartbeat once [us]");
DECLARE_COUNTER(arangodb_ioheartbeat_failures_total,
                "Total number of failures in IO heartbeat");
DECLARE_COUNTER(arangodb_ioheartbeat_delays_total,
                "Total number of delays in IO heartbeat");

/// IO check thread main loop
/// The purpose of this thread is to try to perform a simple IO write
/// operation on the database volume regularly. We need visibility in
/// production if IO is slow or not possible at all.
IOHeartbeatThread::IOHeartbeatThread(Server& server,
                                     metrics::MetricsFeature& metricsFeature)
    : ServerThread<ArangodServer>(server, "IOHeartbeat"),
      _exeTimeHistogram(metricsFeature.add(arangodb_ioheartbeat_duration{})),
      _failures(metricsFeature.add(arangodb_ioheartbeat_failures_total{})),
      _delays(metricsFeature.add(arangodb_ioheartbeat_delays_total{})) {}

IOHeartbeatThread::~IOHeartbeatThread() { shutdown(); }

void IOHeartbeatThread::run() {
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  std::string testFilePath = FileUtils::buildFilename(
      databasePathFeature.directory(), "TestFileIOHeartbeat");
  std::string testFileContent = "This is just an I/O test.\n";

  LOG_TOPIC("66665", DEBUG, Logger::ENGINES) << "IOHeartbeatThread: running...";

  while (true) {
    try {  // protect thread against any exceptions
      if (isStopping()) {
        // done
        break;
      }

      LOG_TOPIC("66659", DEBUG, Logger::ENGINES)
          << "IOHeartbeat: testing to write/read/remove " << testFilePath;
      // We simply write a file and sync it to disk in the database
      // directory and then read it and then delete it again:
      auto start1 = std::chrono::steady_clock::now();
      bool trouble = false;
      try {
        FileUtils::spit(testFilePath, testFileContent, true);
      } catch (std::exception const& exc) {
        ++_failures;
        LOG_TOPIC("66663", INFO, Logger::ENGINES)
            << "IOHeartbeat: exception when writing test file: " << exc.what();
        trouble = true;
      }
      auto finish = std::chrono::steady_clock::now();
      std::chrono::duration<double> dur = finish - start1;
      bool delayed = dur > std::chrono::seconds(1);
      if (trouble || delayed) {
        if (delayed) {
          ++_delays;
        }
        LOG_TOPIC("66662", INFO, Logger::ENGINES)
            << "IOHeartbeat: trying to write test file took "
            << std::chrono::duration_cast<std::chrono::microseconds>(dur)
                   .count()
            << " microseconds.";
      }

      // Read the file if we can reasonably assume it is there:
      if (!trouble) {
        auto start = std::chrono::steady_clock::now();
        try {
          std::string content = FileUtils::slurp(testFilePath);
          if (content != testFileContent) {
            LOG_TOPIC("66660", INFO, Logger::ENGINES)
                << "IOHeartbeat: read content of test file was not as "
                   "expected, found:'"
                << content << "', expected: '" << testFileContent << "'";
            trouble = true;
            ++_failures;
          }
        } catch (std::exception const& exc) {
          ++_failures;
          LOG_TOPIC("66661", INFO, Logger::ENGINES)
              << "IOHeartbeat: exception when reading test file: "
              << exc.what();
          trouble = true;
        }
        auto finish = std::chrono::steady_clock::now();
        std::chrono::duration<double> dur = finish - start;
        bool delayed = dur > std::chrono::seconds(1);
        if (trouble || delayed) {
          if (delayed) {
            ++_delays;
          }
          LOG_TOPIC("66669", INFO, Logger::ENGINES)
              << "IOHeartbeat: trying to read test file took "
              << std::chrono::duration_cast<std::chrono::microseconds>(dur)
                     .count()
              << " microseconds.";
        }

        // And remove it again:
        start = std::chrono::steady_clock::now();
        ErrorCode err = FileUtils::remove(testFilePath);
        if (err != TRI_ERROR_NO_ERROR) {
          ++_failures;
          LOG_TOPIC("66670", INFO, Logger::ENGINES)
              << "IOHeartbeat: error when removing test file: " << err;
          trouble = true;
        }
        finish = std::chrono::steady_clock::now();
        dur = finish - start;
        delayed = dur > std::chrono::seconds(1);
        if (trouble || delayed) {
          if (delayed) {
            ++_delays;
          }
          LOG_TOPIC("66671", INFO, Logger::ENGINES)
              << "IOHeartbeat: trying to remove test file took "
              << std::chrono::duration_cast<std::chrono::microseconds>(dur)
                     .count()
              << " microseconds.";
        }
      }

      // Total duration and update histogram:
      dur = finish - start1;
      _exeTimeHistogram.count(static_cast<double>(
          std::chrono::duration_cast<std::chrono::microseconds>(dur).count()));

      std::unique_lock<std::mutex> guard(_mutex);
      if (trouble) {
        // In case of trouble, we retry more quickly, since we want to
        // have a record when the trouble has actually stopped!
        _cv.wait_for(guard, checkIntervalTrouble);
      } else {
        _cv.wait_for(guard, checkIntervalNormal);
      }
    } catch (...) {
    }
    // next iteration
  }
  LOG_TOPIC("66664", DEBUG, Logger::ENGINES) << "IOHeartbeatThread: stopped.";
}

DatabaseFeature::DatabaseFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(false);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<AuthenticationFeature>();
  startsAfter<CacheManagerFeature>();
  startsAfter<EngineSelectorFeature>();
  startsAfter<InitDatabaseFeature>();
  startsAfter<StorageEngineFeature>();
}

void DatabaseFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "database options");

  options
      ->addOption("--database.default-replication-version",
                  "default replication version, can be overwritten "
                  "when creating a new database, possible values: 1, 2",
                  new replication::ReplicationVersionParameter(
                      &_defaultReplicationVersion),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(31100);

  options->addOption(
      "--database.wait-for-sync",
      "The default waitForSync behavior. Can be overwritten when creating a "
      "collection.",
      new BooleanParameter(&_defaultWaitForSync),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  // the following option was obsoleted in 3.9
  options->addObsoleteOption(
      "--database.force-sync-properties",
      "Force syncing of collection properties to disk after creating a "
      "collection or updating its properties. Otherwise, let the waitForSync "
      "property of each collection determine it.",
      false);

  options->addOption(
      "--database.ignore-datafile-errors",
      "Load collections even if datafiles may contain errors.",
      new BooleanParameter(&_ignoreDatafileErrors),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--database.extended-names-databases",
                  "Allow most UTF-8 characters in database names. Once in use, "
                  "this option cannot be turned off again.",
                  new BooleanParameter(&_extendedNamesForDatabases),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(30900);

  options
      ->addOption("--database.io-heartbeat",
                  "Perform I/O heartbeat to test the underlying volume.",
                  new BooleanParameter(&_performIOHeartbeat),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30807);

  // the following option was obsoleted in 3.9
  options->addObsoleteOption(
      "--database.old-system-collections",
      "Create and use deprecated system collection (_modules, _fishbowl).",
      false);

  // the following option was obsoleted in 3.8
  options->addObsoleteOption(
      "--database.throw-collection-not-loaded-error",
      "throw an error when accessing a collection that is still loading",
      false);

  // the following option was removed in 3.7
  options->addObsoleteOption(
      "--database.maximal-journal-size",
      "default maximal journal size, can be overwritten when "
      "creating a collection",
      true);

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

void DatabaseFeature::initCalculationVocbase(ArangodServer& server) {
  calculationVocbase =
      std::make_unique<TRI_vocbase_t>(createExpressionVocbaseInfo(server));
}

void DatabaseFeature::start() {
  if (_extendedNamesForDatabases) {
    LOG_TOPIC("2c0c6", WARN, arangodb::Logger::FIXME)
        << "Extended names for databases are an experimental feature which "
           "can "
        << "cause incompatibility issues with not-yet-prepared drivers and "
           "applications - do not use in production!";
  }

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
  _databaseManager = std::make_unique<DatabaseManagerThread>(server());

  if (!_databaseManager->start()) {
    LOG_TOPIC("7eb06", FATAL, arangodb::Logger::FIXME)
        << "could not start database manager thread";
    FATAL_ERROR_EXIT();
  }

  // start IOHeartbeat thread:
  if ((ServerState::instance()->isDBServer() ||
       ServerState::instance()->isSingleServer() ||
       ServerState::instance()->isAgent()) &&
      _performIOHeartbeat) {
    _ioHeartbeatThread = std::make_unique<IOHeartbeatThread>(
        server(), server().getFeature<metrics::MetricsFeature>());
    if (!_ioHeartbeatThread->start()) {
      LOG_TOPIC("7eb07", FATAL, arangodb::Logger::FIXME)
          << "could not start IO check thread";
      FATAL_ERROR_EXIT();
    }
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
  if (_ioHeartbeatThread) {
    _ioHeartbeatThread->beginShutdown();  // will set thread state to STOPPING
    _ioHeartbeatThread->wakeup();         // will shorten the wait
  }

  auto databases = _databases.load();

  for (auto& p : *databases) {
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

  if (ServerState::instance()->isCoordinator()) {
    return;
  }

  std::unique_lock lock{_databasesMutex};
  auto databases = _databases.load();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto queryRegistry = QueryRegistryFeature::registry();
  if (queryRegistry != nullptr) {
    TRI_ASSERT(queryRegistry->numberRegisteredQueries() == 0);
  }
#endif

  auto stopVocbase = [](TRI_vocbase_t* vocbase) {
    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // i am here for debugging only.
    currentVocbase = vocbase;
    static size_t currentCursorCount =
        currentVocbase->cursorRepository()->count();
    static size_t currentQueriesCount = currentVocbase->queryList()->count();

    LOG_TOPIC("840a4", DEBUG, Logger::FIXME)
        << "shutting down database " << currentVocbase->name() << ": "
        << (void*)currentVocbase << ", cursors: " << currentCursorCount
        << ", queries: " << currentQueriesCount;
#endif
    vocbase->stop();

    vocbase->processCollectionsOnShutdown([](LogicalCollection* collection) {
      // no one else must modify the collection's status while we are in
      // here
      collection->executeWhileStatusWriteLocked(
          [collection]() { collection->close(); });
    });

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // i am here for debugging only.
    LOG_TOPIC("4b2b7", DEBUG, Logger::FIXME)
        << "shutting down database " << currentVocbase->name() << ": "
        << (void*)currentVocbase << " successful";
#endif
  };

  for (auto& [name, vocbase] : *databases) {
    stopVocbase(vocbase);
  }

  for (auto& vocbase : _droppedDatabases) {
    stopVocbase(vocbase);
  }

  lock.unlock();

  // flush again so we are sure no query is left in the cache here
  arangodb::aql::QueryCache::instance()->invalidate();
}

void DatabaseFeature::unprepare() {
  // delete the IO checker thread
  if (_ioHeartbeatThread != nullptr) {
    _ioHeartbeatThread->beginShutdown();

    while (_ioHeartbeatThread->isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

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

  _ioHeartbeatThread.reset();
  _databaseManager.reset();

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // This is to avoid heap use after free errors in the iresearch tests,
  // because the destruction a callback uses a database. I don't know if this
  // is safe to do, thus I enclosed it in ARANGODB_USE_GOOGLE_TESTS to prevent
  // accidentally breaking anything. However,
  // TODO Find out if this is okay and may be merged (maybe without the
  // #ifdef), or if this has to be done differently in the tests instead. The
  // errors may also go away when some new PR is merged, so maybe this can
  // just be removed in the future.
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

  if (!ServerState::instance()->isCoordinator()) {
    auto databases = _databases.load();

    for (auto& p : *databases) {
      TRI_vocbase_t* vocbase = p.second;
      // iterate over all databases
      TRI_ASSERT(vocbase != nullptr);

      if (vocbase->replicationApplier()) {
        if (server().hasFeature<ReplicationFeature>()) {
          server().getFeature<ReplicationFeature>().startApplier(vocbase);
        }
      }
    }
  }
}

Result DatabaseFeature::registerPostRecoveryCallback(
    std::function<Result()>&& callback) {
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

void DatabaseFeature::enumerate(
    std::function<void(TRI_vocbase_t*)> const& callback) {
  auto databases = _databases.load();

  for (auto& p : *databases) {
    callback(p.second);
  }
}

/// @brief create a new database
Result DatabaseFeature::createDatabase(CreateDatabaseInfo&& info,
                                       TRI_vocbase_t*& result) {
  std::string name = info.getName();
  auto dbId = info.getId();
  VPackBuilder markerBuilder;
  {
    VPackObjectBuilder guard(&markerBuilder);
    info.toVelocyPack(markerBuilder);  // can we improve this
  }
  result = nullptr;

  bool extendedNames = extendedNamesForDatabases();
  if (!DatabaseNameValidator::isAllowedName(/*allowSystem*/ false,
                                            extendedNames, name)) {
    return {TRI_ERROR_ARANGO_DATABASE_NAME_INVALID};
  }

  std::unique_ptr<TRI_vocbase_t> vocbase;

  // create database in storage engine
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  // the create lock makes sure no one else is creating a database while we're
  // inside this function
  std::lock_guard lockCreate{_databaseCreateLock};
  {
    {
      auto databases = _databases.load();

      auto it = databases->find(name);
      if (it != databases->end()) {
        // name already in use
        return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME,
                      std::string("duplicate database name '") + name + "'");
      }
    }

    // createDatabase must return a valid database or throw
    vocbase = engine.createDatabase(std::move(info));
    TRI_ASSERT(vocbase != nullptr);

    if (!ServerState::instance()->isCoordinator()) {
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
      vocbase->_deadlockDetector.enabled(
          !ServerState::instance()->isRunningInCluster());

      // create application directories
      V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
      auto appPath = dealer.appPath();

      // create app directory for database if it does not exist
      std::string const dirName =
          ::getDatabaseDirName(name, std::to_string(dbId));
      auto res = createApplicationDirectory(dirName, appPath, true);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    if (!engine.inRecovery()) {
      if (!ServerState::instance()->isCoordinator() &&
          server().hasFeature<ReplicationFeature>()) {
        server().getFeature<ReplicationFeature>().startApplier(vocbase.get());
      }

      // increase reference counter
      bool result = vocbase->use();
      TRI_ASSERT(result);
    }

    std::lock_guard lock{_databasesMutex};
    auto prev = _databases.load();

    auto next = _databases.make(prev);
    next->insert(std::make_pair(name, vocbase.get()));

    _databases.store(std::move(next));
    waitUnique(prev);
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
ErrorCode DatabaseFeature::dropDatabase(std::string_view name,
                                        bool removeAppsDirectory) {
  if (name == StaticStrings::SystemDatabase) {
    // prevent deletion of system database
    return TRI_ERROR_FORBIDDEN;
  }

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  auto res = TRI_ERROR_NO_ERROR;
  {
    std::lock_guard lock{_databasesMutex};

    auto prev = _databases.load();
    decltype(_databases.create()) next;

    auto it = prev->find(name);

    if (it == prev->end()) {
      return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
    }

    TRI_vocbase_t* vocbase = it->second;

    // call LogicalDataSource::drop() to allow instances to clean up
    // internal state (e.g. for LogicalView implementations)
    TRI_vocbase_t::dataSourceVisitor visitor =
        [&res, &name](arangodb::LogicalDataSource& dataSource) -> bool {
      // skip LogicalCollection since their internal state is always in the
      // StorageEngine (optimization)
      if (arangodb::LogicalDataSource::Category::kCollection ==
          dataSource.category()) {
        return true;
      }

      auto result = dataSource.drop();

      if (!result.ok()) {
        res = result.errorNumber();
        LOG_TOPIC("c44cb", ERR, arangodb::Logger::FIXME)
            << "failed to drop DataSource '" << dataSource.name()
            << "' while dropping database '" << name
            << "': " << result.errorNumber() << " " << result.errorMessage();
      }

      return true;  // try next DataSource
    };

    // acquires a write lock to avoid potential deadlocks
    vocbase->visitDataSources(visitor);

    if (TRI_ERROR_NO_ERROR != res) {
      return res;
    }

    next = _databases.make(prev);
    next->erase(name);

    _droppedDatabases.insert(vocbase);

    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->id() != 0);

    _databases.store(std::move(next));
    waitUnique(prev);

    TRI_ASSERT(!vocbase->isSystem());
    // mark as deleted
    // it is fine to do this here already, because the vocbase cannot be deleted
    // while we are still holding the _databasesMutex. the DatabaseManagerThread
    // can only pick a database for deletion once it is in _droppedDatabases
    // *AND* it has acquired the _databasesMutex.
    bool result = vocbase->markAsDropped();
    TRI_ASSERT(result);

    vocbase->setIsOwnAppsDirectory(removeAppsDirectory);

    // invalidate all entries for the database
    arangodb::aql::QueryCache::instance()->invalidate(vocbase);

    if (server().hasFeature<arangodb::iresearch::IResearchAnalyzerFeature>()) {
      server()
          .getFeature<arangodb::iresearch::IResearchAnalyzerFeature>()
          .invalidate(*vocbase);
    }

    auto queryRegistry = QueryRegistryFeature::registry();
    if (queryRegistry != nullptr) {
      queryRegistry->destroy(vocbase->name());
    }
    // TODO Temporary fix, this full method needs to be unified.
    try {
      vocbase->cursorRepository()->garbageCollect(true);
    } catch (...) {
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
ErrorCode DatabaseFeature::dropDatabase(TRI_voc_tick_t id,
                                        bool removeAppsDirectory) {
  std::string name;

  // find database by name
  {
    auto databases = _databases.load();

    for (auto& p : *databases) {
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

std::vector<TRI_voc_tick_t> DatabaseFeature::getDatabaseIds(
    bool includeSystem) {
  std::vector<TRI_voc_tick_t> ids;

  {
    auto databases = _databases.load();

    ids.reserve(databases->size());

    for (auto& p : *databases) {
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
    auto databases = _databases.load();

    names.reserve(databases->size());

    for (auto& p : *databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase->isDropped()) {
        continue;
      }
      names.emplace_back(vocbase->name());
    }
  }

  std::sort(names.begin(), names.end());

  return names;
}

/// @brief return the list of all database names for a user
std::vector<std::string> DatabaseFeature::getDatabaseNamesForUser(
    std::string const& username) {
  std::vector<std::string> names;

  AuthenticationFeature* af = AuthenticationFeature::instance();
  {
    auto databases = _databases.load();

    for (auto& p : *databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase->isDropped()) {
        continue;
      }

      if (af->isActive() && af->userManager() != nullptr) {
        auto level =
            af->userManager()->databaseAuthLevel(username, vocbase->name());
        if (level == auth::Level::NONE) {  // hide dbs without access
          continue;
        }
      }

      names.emplace_back(vocbase->name());
    }
  }

  std::sort(names.begin(), names.end());

  return names;
}

/// @brief return the list of all database names
void DatabaseFeature::inventory(
    VPackBuilder& result, TRI_voc_tick_t maxTick,
    std::function<bool(arangodb::LogicalCollection const*)> const& nameFilter) {
  result.openObject();
  {
    auto databases = _databases.load();

    for (auto& p : *databases) {
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

VocbasePtr DatabaseFeature::useDatabase(std::string_view name) const {
  auto databases = _databases.load();

  if (auto const it = databases->find(name); it != databases->end()) {
    TRI_vocbase_t* vocbase = it->second;
    if (vocbase->use()) {
      return VocbasePtr{vocbase};
    }
  }

  return nullptr;
}

VocbasePtr DatabaseFeature::useDatabase(TRI_voc_tick_t id) const {
  auto databases = _databases.load();

  for (auto& p : *databases) {
    TRI_vocbase_t* vocbase = p.second;

    if (vocbase->id() == id) {
      if (vocbase->use()) {
        return VocbasePtr{vocbase};
      }
      break;
    }
  }

  return nullptr;
}

/// @brief lookup a database by its name, not increasing its reference count
TRI_vocbase_t* DatabaseFeature::lookupDatabase(std::string_view name) const {
  if (name.empty()) {
    return nullptr;
  }

  auto databases = _databases.load();

  // database names with a number in front are invalid names
  if (name[0] >= '0' && name[0] <= '9') {
    TRI_voc_tick_t id = NumberUtils::atoi_zero<TRI_voc_tick_t>(
        name.data(), name.data() + name.size());
    for (auto& p : *databases) {
      TRI_vocbase_t* vocbase = p.second;
      if (vocbase->id() == id) {
        return vocbase;
      }
    }
  } else {
    for (auto& p : *databases) {
      TRI_vocbase_t* vocbase = p.second;
      if (name == vocbase->name()) {
        return vocbase;
      }
    }
  }

  return nullptr;
}

std::string DatabaseFeature::translateCollectionName(
    std::string_view dbName, std::string_view collectionName) {
  auto databases = _databases.load();
  auto itr = databases->find(dbName);

  if (itr == databases->end()) {
    return std::string();
  }

  auto* vocbase = itr->second;
  TRI_ASSERT(vocbase != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    CollectionNameResolver resolver(*vocbase);

    return resolver.getCollectionNameCluster(
        DataSourceId{NumberUtils::atoi_zero<DataSourceId::BaseType>(
            collectionName.data(),
            collectionName.data() + collectionName.size())});
  } else {
    auto collection = vocbase->lookupCollection(collectionName);

    return collection ? collection->name() : std::string();
  }
}

void DatabaseFeature::enumerateDatabases(
    std::function<void(TRI_vocbase_t& vocbase)> const& func) {
  auto databases = _databases.load();

  for (auto& p : *databases) {
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

  ReplicationFeature& replicationFeature =
      server().getFeature<ReplicationFeature>();

  std::lock_guard lock{_databasesMutex};
  auto databases = _databases.load();

  for (auto& p : *databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    if (!ServerState::instance()->isCoordinator()) {
      replicationFeature.stopApplier(vocbase);
    }
  }
}

/// @brief close all opened databases
void DatabaseFeature::closeOpenDatabases() {
  std::unique_lock lock{_databasesMutex};

  // Build the new value:
  auto databases = _databases.load();

  // Replace the old by the new:
  _databases.store(_databases.create());
  waitUnique(databases);
  lock.unlock();

  // Now it is safe to destroy the old databases:
  for (auto& p : *databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    vocbase->shutdown();

    delete vocbase;
  }
}

/// @brief create base app directory
ErrorCode DatabaseFeature::createBaseApplicationDirectory(
    std::string const& appPath, std::string const& type) {
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
            << "someone else created base application directory '" << path
            << "'";
        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  return res;
}

/// @brief create app subdirectory for a database
ErrorCode DatabaseFeature::createApplicationDirectory(
    std::string const& name, std::string const& basePath, bool removeExisting) {
  if (basePath.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
  if (!dealer.isEnabled()) {
    // no JavaScript enabled - no need to create the js/apps directory/ies
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
  auto res =
      TRI_CreateRecursiveDirectory(path.c_str(), systemError, errorMessage);

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
  std::lock_guard lock{_databasesMutex};

  auto prev = _databases.load();
  auto next = _databases.make(prev);

  ServerState::RoleEnum role = arangodb::ServerState::instance()->getRole();

  for (VPackSlice it : VPackArrayIterator(databases)) {
    TRI_ASSERT(it.isObject());
    LOG_TOPIC("95f68", TRACE, Logger::FIXME)
        << "processing database: " << it.toJson();

    VPackSlice deleted = it.get("deleted");
    if (deleted.isBoolean() && deleted.getBoolean()) {
      // ignore deleted databases here
      continue;
    }

    std::string const databaseName = it.get("name").copyString();
    std::string const id = VelocyPackHelper::getStringValue(it, "id", "");
    std::string const dirName = ::getDatabaseDirName(databaseName, id);

    // create app directory for database if it does not exist
    res = createApplicationDirectory(dirName, appPath, false);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }

    // open the database and scan collections in it

    // try to open this database
    arangodb::CreateDatabaseInfo info(server(), ExecContext::current());
    auto res = info.load(it, VPackSlice::emptyArraySlice());
    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID)) {
        // special case: if we find an invalid database name during startup,
        // we will give the user some hint how to fix it
        std::string errorMsg(res.errorMessage());
        errorMsg.append(": '").append(databaseName).append("'");
        // check if the name would be allowed when using extended names
        if (DatabaseNameValidator::isAllowedName(
                /*isSystem*/ false, /*extendedNames*/ true, databaseName)) {
          errorMsg.append(
              ". This database name would be allowed when using the "
              "extended naming convention for databases, which is "
              "currently disabled. The extended naming convention can "
              "be enabled via the startup option "
              "`--database.extended-names-databases true`");
        }
        res.reset(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID, std::move(errorMsg));
      }
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
    next->insert(std::make_pair(database->name(), database.get()));
    database.release();
  }

  _databases.store(std::move(next));
  waitUnique(prev);

  return res;
}

/// @brief close all dropped databases
void DatabaseFeature::closeDroppedDatabases() {
  std::unique_lock lock{_databasesMutex};
  // take a copy
  auto dropped = std::move(_droppedDatabases);
  _droppedDatabases.clear();
  lock.unlock();

  auto guard = scopeGuard([&dropped]() noexcept {
    for (auto p : dropped) {
      delete p;
    }
  });

  if (!ServerState::instance()->isCoordinator()) {
    return;
  }

  for (auto p : dropped) {
    p->shutdown();
  }
}

void DatabaseFeature::verifyAppPaths() {
  // create shared application directory js/apps
  V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
  if (!dealer.isEnabled()) {
    // no JavaScript enabled - no need to create the js/apps directory/ies
    return;
  }

  auto appPath = dealer.appPath();

  if (!appPath.empty() && !TRI_IsDirectory(appPath.c_str())) {
    long systemError;
    std::string errorMessage;
    auto res = TRI_CreateRecursiveDirectory(appPath.c_str(), systemError,
                                            errorMessage);

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
  auto databases = _databases.load();

  for (auto& p : *databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);

    vocbase->_deadlockDetector.enabled(true);
  }
}
