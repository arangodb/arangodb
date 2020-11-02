////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef APPLICATION_FEATURES_DATABASE_FEATURE_H
#define APPLICATION_FEATURES_DATABASE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/DataProtector.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "Utils/VersionTracker.h"
#include "VocBase/voc-types.h"
#include "VocBase/Methods/Databases.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class LogicalCollection;
}

namespace arangodb {
namespace velocypack {
class Builder; // forward declaration
class Slice; // forward declaration
} // velocypack
} //arangodb

namespace arangodb {

class DatabaseManagerThread final : public Thread {
 public:
  DatabaseManagerThread(DatabaseManagerThread const&) = delete;
  DatabaseManagerThread& operator=(DatabaseManagerThread const&) = delete;

  explicit DatabaseManagerThread(application_features::ApplicationServer&);
  ~DatabaseManagerThread();

  void run() override;

 private:
  // how long will the thread pause between iterations
  static constexpr unsigned long waitTime() { return static_cast<unsigned long>(500U * 1000U); }
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
  void prepare() override final;

  // used by catch tests
#ifdef ARANGODB_USE_GOOGLE_TESTS
  inline int loadDatabases(velocypack::Slice const& databases) {
    return iterateDatabases(databases);
  }
#endif

  /// @brief will be called when the recovery phase has run
  /// this will call the engine-specific recoveryDone() procedures
  /// and will execute engine-unspecific operations (such as starting
  /// the replication appliers) for all databases
  void recoveryDone();

  /// @brief enumerate all databases
  void enumerate(std::function<void(TRI_vocbase_t*)> const& callback);

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

  Result createDatabase(arangodb::CreateDatabaseInfo&& , TRI_vocbase_t*& result);

  int dropDatabase(std::string const& name, bool removeAppsDirectory);
  int dropDatabase(TRI_voc_tick_t id, bool removeAppsDirectory);

  void inventory(arangodb::velocypack::Builder& result, TRI_voc_tick_t,
                 std::function<bool(arangodb::LogicalCollection const*)> const& nameFilter);

  TRI_vocbase_t* useDatabase(std::string const& name) const;
  TRI_vocbase_t* useDatabase(TRI_voc_tick_t id) const;

  TRI_vocbase_t* lookupDatabase(std::string const& name) const;
  void enumerateDatabases(std::function<void(TRI_vocbase_t& vocbase)> const& func);
  std::string translateCollectionName(std::string const& dbName,
                                      std::string const& collectionName);

  bool ignoreDatafileErrors() const { return _ignoreDatafileErrors; }
  bool isInitiallyEmpty() const { return _isInitiallyEmpty; }
  bool checkVersion() const { return _checkVersion; }
  bool upgrade() const { return _upgrade; }
  bool useOldSystemCollections() const { return _useOldSystemCollections; }
  bool forceSyncProperties() const { return _forceSyncProperties; }
  void forceSyncProperties(bool value) { _forceSyncProperties = value; }
  bool waitForSync() const { return _defaultWaitForSync; }

  void enableCheckVersion() { _checkVersion = true; }
  void enableUpgrade() { _upgrade = true; }
  void disableUpgrade() { _upgrade = false; }
  void isInitiallyEmpty(bool value) { _isInitiallyEmpty = value; }
  
  struct DatabasesLists {
    std::unordered_map<std::string, TRI_vocbase_t*> _databases;
    std::unordered_set<TRI_vocbase_t*> _droppedDatabases;
  };

  static TRI_vocbase_t& getCalculationVocbase();
  

 private:
  static void initCalculationVocbase(application_features::ApplicationServer& server);

  void stopAppliers();

  /// @brief create base app directory
  int createBaseApplicationDirectory(std::string const& appPath, std::string const& type);

  /// @brief create app subdirectory for a database
  int createApplicationDirectory(std::string const& name, std::string const& basePath, bool removeExisting);

  /// @brief iterate over all databases in the databases directory and open them
  int iterateDatabases(arangodb::velocypack::Slice const& databases);

  /// @brief close all opened databases
  void closeOpenDatabases();

  /// @brief close all dropped databases
  void closeDroppedDatabases();

  void verifyAppPaths();

  /// @brief activates deadlock detection in all existing databases
  void enableDeadlockDetection();

  bool _defaultWaitForSync;
  bool _forceSyncProperties;
  bool _ignoreDatafileErrors;

  std::unique_ptr<DatabaseManagerThread> _databaseManager;

  std::atomic<DatabasesLists*> _databasesLists;
  // TODO: Make this again a template once everybody has gcc >= 4.9.2
  // arangodb::basics::DataProtector<64>
  mutable arangodb::basics::DataProtector _databasesProtector;
  mutable arangodb::Mutex _databasesMutex;

  bool _isInitiallyEmpty;
  bool _checkVersion;
  bool _upgrade;
  bool _useOldSystemCollections;

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
