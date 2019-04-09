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
#include "Utils/VersionTracker.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;

namespace aql {
class QueryRegistry;
}

class DatabaseManagerThread final : public Thread {
 public:
  DatabaseManagerThread(DatabaseManagerThread const&) = delete;
  DatabaseManagerThread& operator=(DatabaseManagerThread const&) = delete;

  DatabaseManagerThread();
  ~DatabaseManagerThread();

  void run() override;

 private:
  // how long will the thread pause between iterations
  static constexpr unsigned long waitTime() { return 500 * 1000; }
};

class DatabaseFeature : public application_features::ApplicationFeature {
  friend class DatabaseManagerThread;

 public:
  static DatabaseFeature* DATABASE;

  explicit DatabaseFeature(application_features::ApplicationServer& server);
  ~DatabaseFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  /// @brief will be called when the recovery phase has run
  /// this will call the engine-specific recoveryDone() procedures
  /// and will execute engine-unspecific operations (such as starting
  /// the replication appliers) for all databases
  void recoveryDone();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief register a callback
  ///        if StorageEngine.inRecovery() -> call at start of recoveryDone()
  ///                                         and fail recovery if callback
  ///                                         !ok()
  ///        if !StorageEngine.inRecovery() -> call immediately and return
  ///                                          result
  //////////////////////////////////////////////////////////////////////////////
  Result registerPostRecoveryCallback(std::function<Result()>&& callback);

  VersionTracker* versionTracker() { return &_versionTracker; }

  /// @brief get the ids of all local databases
  std::vector<TRI_voc_tick_t> getDatabaseIds(bool includeSystem);
  std::vector<std::string> getDatabaseNames();
  std::vector<std::string> getDatabaseNamesForUser(std::string const& user);

  int createDatabase(TRI_voc_tick_t id, std::string const& name, TRI_vocbase_t*& result);
  int dropDatabase(std::string const& name, bool waitForDeletion, bool removeAppsDirectory);
  int dropDatabase(TRI_voc_tick_t id, bool waitForDeletion, bool removeAppsDirectory);

  void inventory(arangodb::velocypack::Builder& result, TRI_voc_tick_t,
                 std::function<bool(arangodb::LogicalCollection const*)> const& nameFilter);

  TRI_vocbase_t* useDatabase(std::string const& name);
  TRI_vocbase_t* useDatabase(TRI_voc_tick_t id);

  TRI_vocbase_t* lookupDatabase(std::string const& name);
  void enumerateDatabases(std::function<void(TRI_vocbase_t& vocbase)> const& func);
  std::string translateCollectionName(std::string const& dbName,
                                      std::string const& collectionName);

  bool ignoreDatafileErrors() const { return _ignoreDatafileErrors; }
  bool isInitiallyEmpty() const { return _isInitiallyEmpty; }
  bool checkVersion() const { return _checkVersion; }
  bool upgrade() const { return _upgrade; }
  bool forceSyncProperties() const { return _forceSyncProperties; }
  void forceSyncProperties(bool value) { _forceSyncProperties = value; }
  bool waitForSync() const { return _defaultWaitForSync; }
  uint64_t maximalJournalSize() const { return _maximalJournalSize; }

  void enableCheckVersion() { _checkVersion = true; }
  void enableUpgrade() { _upgrade = true; }
  bool throwCollectionNotLoadedError() const {
    return _throwCollectionNotLoadedError.load(std::memory_order_relaxed);
  }
  void throwCollectionNotLoadedError(bool value) {
    _throwCollectionNotLoadedError.store(value);
  }
  void isInitiallyEmpty(bool value) { _isInitiallyEmpty = value; }

 private:
  void stopAppliers();
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

  uint64_t _maximalJournalSize;
  bool _defaultWaitForSync;
  bool _forceSyncProperties;
  bool _ignoreDatafileErrors;
  std::atomic<bool> _throwCollectionNotLoadedError;

  std::unique_ptr<DatabaseManagerThread> _databaseManager;

  std::atomic<DatabasesLists*> _databasesLists;
  // TODO: Make this again a template once everybody has gcc >= 4.9.2
  // arangodb::basics::DataProtector<64>
  arangodb::basics::DataProtector _databasesProtector;
  arangodb::Mutex _databasesMutex;

  bool _isInitiallyEmpty;
  bool _checkVersion;
  bool _upgrade;

  /// @brief lock for serializing the creation of databases
  arangodb::Mutex _databaseCreateLock;

  std::vector<std::function<Result()>> _pendingRecoveryCallbacks;

  /// @brief a simple version tracker for all database objects
  /// maintains a global counter that is increased on every modification
  /// (addition, removal, change) of database objects
  VersionTracker _versionTracker;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // i am here for debugging only.
  static TRI_vocbase_t* CURRENT_VOCBASE;
#endif
};

}  // namespace arangodb

#endif
