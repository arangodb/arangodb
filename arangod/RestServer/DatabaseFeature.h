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

#ifndef APPLICATION_FEATURES_DATABASE_FEATURE_H
#define APPLICATION_FEATURES_DATABASE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/DataProtector.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {

namespace aql {
class QueryRegistry;
}

class DatabaseManagerThread : public Thread {
 public:
  DatabaseManagerThread(DatabaseManagerThread const&) = delete;
  DatabaseManagerThread& operator=(DatabaseManagerThread const&) = delete;

  DatabaseManagerThread(); 
  ~DatabaseManagerThread();

  void run() override;

 private:
  // how long will the thread pause between iterations
  static constexpr unsigned long waitTime() {
    return 500 * 1000;
  }
};

class DatabaseFeature final : public application_features::ApplicationFeature {
 friend class DatabaseManagerThread;

 public:
  static DatabaseFeature* DATABASE;

 public:
  explicit DatabaseFeature(application_features::ApplicationServer* server);
  ~DatabaseFeature();

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  /// @brief will be called when the recovery phase has run
  /// this will call the engine-specific recoveryDone() procedures
  /// and will execute engine-unspecific operations (such as starting
  /// the replication appliers) for all databases
  void recoveryDone();

 public:

   /// @brief get the ids of all local coordinator databases
  std::vector<TRI_voc_tick_t> getDatabaseIdsCoordinator(bool includeSystem);
  std::vector<TRI_voc_tick_t> getDatabaseIds(bool includeSystem);
  std::vector<std::string> getDatabaseNames();
  std::vector<std::string> getDatabaseNamesForUser(std::string const& user);

  int createDatabaseCoordinator(TRI_voc_tick_t id, std::string const& name, TRI_vocbase_t*& result);
  int createDatabase(TRI_voc_tick_t id, std::string const& name, TRI_vocbase_t*& result);
  int dropDatabaseCoordinator(TRI_voc_tick_t id, bool force);
  int dropDatabase(std::string const& name, bool waitForDeletion, bool removeAppsDirectory);
  int dropDatabase(TRI_voc_tick_t id, bool waitForDeletion, bool removeAppsDirectory);

  TRI_vocbase_t* useDatabaseCoordinator(std::string const& name);
  TRI_vocbase_t* useDatabaseCoordinator(TRI_voc_tick_t id);
  TRI_vocbase_t* useDatabase(std::string const& name);
  TRI_vocbase_t* useDatabase(TRI_voc_tick_t id);

  TRI_vocbase_t* lookupDatabaseCoordinator(std::string const& name);
  TRI_vocbase_t* lookupDatabase(std::string const& name);
  void enumerateDatabases(std::function<void(TRI_vocbase_t*)>);

  void useSystemDatabase();
  TRI_vocbase_t* systemDatabase() const { return _vocbase; }
  bool ignoreDatafileErrors() const { return _ignoreDatafileErrors; }
  bool isInitiallyEmpty() const { return _isInitiallyEmpty; }
  bool checkVersion() const { return _checkVersion; }
  bool upgrade() const { return _upgrade; }
  bool forceSyncProperties() const { return _forceSyncProperties; }
  void forceSyncProperties(bool value) { _forceSyncProperties = value; }
  bool waitForSync() const { return _defaultWaitForSync; }
  uint64_t maximalJournalSize() const { return _maximalJournalSize; }

  void disableReplicationApplier() { _replicationApplier = false; }
  void enableCheckVersion() { _checkVersion = true; }
  void enableUpgrade() { _upgrade = true; }
  bool throwCollectionNotLoadedError() const { return _throwCollectionNotLoadedError.load(std::memory_order_relaxed); }
  void throwCollectionNotLoadedError(bool value) { _throwCollectionNotLoadedError.store(value); }
  bool check30Revisions() const { return _check30Revisions == "true" || _check30Revisions == "fail"; }
  bool fail30Revisions() const { return _check30Revisions == "fail"; }
  void isInitiallyEmpty(bool value) { _isInitiallyEmpty = value; }

 private:
  void closeDatabases();
  void updateContexts();

  /// @brief create base app directory
  int createBaseApplicationDirectory(std::string const& appPath, std::string const& type);
  
  /// @brief create app subdirectory for a database
  int createApplicationDirectory(std::string const& name, std::string const& basePath);

  /// @brief iterate over all databases in the databases directory and open them
  int iterateDatabases(arangodb::velocypack::Slice const& databases);

  /// @brief close all opened databases
  void closeOpenDatabases();

  /// @brief close all dropped databases
  void closeDroppedDatabases();

  void verifyAppPaths();

  /// @brief activates deadlock detection in all existing databases
  void enableDeadlockDetection();

  

 private:
  uint64_t _maximalJournalSize;
  bool _defaultWaitForSync;
  bool _forceSyncProperties;
  bool _ignoreDatafileErrors;
  std::string _check30Revisions;
  std::atomic<bool> _throwCollectionNotLoadedError;

  TRI_vocbase_t* _vocbase;
  std::unique_ptr<DatabaseManagerThread> _databaseManager;

  std::atomic<DatabasesLists*> _databasesLists; 
  // TODO: Make this again a template once everybody has gcc >= 4.9.2
  // arangodb::basics::DataProtector<64>
  arangodb::basics::DataProtector _databasesProtector;
  arangodb::Mutex _databasesMutex;

  bool _isInitiallyEmpty;
  bool _replicationApplier;
  bool _checkVersion;
  bool _upgrade;

  /// @brief lock for serializing the creation of databases
  arangodb::Mutex _databaseCreateLock;
};
}

#endif
