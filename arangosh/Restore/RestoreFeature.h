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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Utils/ClientManager.h"
#include "Utils/ClientTaskQueue.h"
#include "Utils/ManagedDirectory.h"
#include "Utils/ProgressTracker.h"

namespace arangodb {

namespace basics {
class StringBuffer;
}

namespace httpclient {
class SimpleHttpResult;
}

class ManagedDirectory;

class RestoreFeature final : public application_features::ApplicationFeature {
 public:
  RestoreFeature(application_features::ApplicationServer& server, int& exitCode);

  // for documentation of virtual methods, see `ApplicationFeature`
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;

  /**
   * @brief Returns the feature name (for registration with `ApplicationServer`)
   * @return The name of the feature
   */
  static std::string featureName();
  
  /**
   * @brief Saves a worker error for later handling and clears queued jobs
   * @param error Error from a client worker
   */
  void reportError(Result const& error);

  /**
   * @brief Returns the first error from the worker errors list
   * @return  First error from the list, or OK if none exist
   */
  Result getFirstError() const;


  std::unique_ptr<basics::StringBuffer> leaseBuffer();
  void returnBuffer(std::unique_ptr<basics::StringBuffer> buffer) noexcept;

  /// @brief Holds configuration data to pass between methods
  struct Options {
    std::vector<std::string> collections{};
    std::vector<std::string> views{};
    std::string inputPath{};
    uint64_t chunkSize{1024 * 1024 * 8};
    uint64_t defaultNumberOfShards{1};     // deprecated
    uint64_t defaultReplicationFactor{1};  // deprecated
    std::vector<std::string> numberOfShards;
    std::vector<std::string> replicationFactor;
    uint32_t threadCount{2};
    uint32_t initialConnectRetries{3};
    bool clusterMode{false};
    bool createDatabase{false};
    bool force{false};
    bool forceSameDatabase{false};
    bool allDatabases{false};
    bool ignoreDistributeShardsLikeErrors{false};
    bool importData{true};
    bool importStructure{true};
    bool includeSystemCollections{false};
    bool overwrite{true};
    bool useEnvelope{true};
    bool continueRestore{false};
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    bool failOnUpdateContinueFile{false};
#endif
    bool cleanupDuplicateAttributes{false};
    bool progress{true};
  };

  enum CollectionState {
    UNKNOWN = 0,
    CREATED = 1,
    RESTORING = 2,
    RESTORED = 3,
  };

  struct CollectionStatus {
    CollectionState state{UNKNOWN};
    size_t bytes_acked{0};

    CollectionStatus();
    explicit CollectionStatus(CollectionState state, size_t bytes_acked = 0u);
    explicit CollectionStatus(VPackSlice slice);

    void toVelocyPack(VPackBuilder& builder) const;
  };

  using RestoreProgressTracker = ProgressTracker<CollectionStatus>;

  /// @brief Stores stats about the overall restore progress
  struct Stats {
    std::atomic<uint64_t> totalBatches{0};
    std::atomic<uint64_t> totalSent{0};
    std::atomic<uint64_t> totalCollections{0};
    std::atomic<uint64_t> restoredCollections{0};
    std::atomic<uint64_t> totalRead{0};
  };

  /// @brief shared state for a single collection, can be shared by multiple
  /// RestoreJobs (one RestoreMainJob and x RestoreSendJobs)
  struct SharedState {
    SharedState() : readCompleteInputfile(false) {}

    Mutex mutex;

    /// @brief this contains errors produced by background send operations
    /// (i.e. RestoreSendJobs)
    Result result;

    /// @brief data chunk offsets (start offset, length) of requests that are currently
    /// ongoing. Resumption of the restore must start at the smallest key value contained
    /// in here (note: we also need to track the length of each chunk to test the
    /// resumption of a restore after a crash)
    std::map<size_t, size_t> readOffsets;

    /// @brief whether ot not we have read the complete input data file for the collection
    bool readCompleteInputfile;
  };
  
  /// @brief Stores all necessary data to restore a single collection or shard
  struct RestoreJob {
    RestoreJob(RestoreFeature& feature, 
               RestoreProgressTracker& progressTracker,
               Options const& options, 
               Stats& stats, 
               bool useEnvelope,
               std::string const& collectionName,
               std::shared_ptr<SharedState> sharedState);

    virtual ~RestoreJob();
    
    virtual Result run(arangodb::httpclient::SimpleHttpClient& client) = 0;

    void updateProgress();

    Result sendRestoreData(arangodb::httpclient::SimpleHttpClient& client,
                           size_t readOffset,
                           char const* buffer,
                           size_t bufferSize);
    
    RestoreFeature& feature;
    RestoreProgressTracker& progressTracker;
    Options const& options;
    Stats& stats;
    bool useEnvelope;
    std::string const collectionName;
    std::shared_ptr<SharedState> sharedState;
  };

  struct RestoreMainJob : public RestoreJob {
    RestoreMainJob(ManagedDirectory& directory, 
                   RestoreFeature& feature, 
                   RestoreProgressTracker& progressTracker,
                   Options const& options, 
                   Stats& stats, 
                   VPackSlice parameters,
                   bool useEnvelope);
    
    Result run(arangodb::httpclient::SimpleHttpClient& client) override;

    Result dispatchRestoreData(arangodb::httpclient::SimpleHttpClient& client,
                               size_t readOffset,
                               char const* data,
                               size_t length,
                               bool forceDirect);
    
    Result restoreData(arangodb::httpclient::SimpleHttpClient& client);

    /// @brief Restore a collection's indexes given its description
    Result restoreIndexes(arangodb::httpclient::SimpleHttpClient& client);

    /// @brief Send command to restore a collection's indexes
    Result sendRestoreIndexes(arangodb::httpclient::SimpleHttpClient& client,
                              arangodb::velocypack::Slice);
    
    ManagedDirectory& directory;
    VPackSlice parameters;
  };
  
  struct RestoreSendJob : public RestoreJob {
    RestoreSendJob(RestoreFeature& feature, 
                   RestoreProgressTracker& progressTracker,
                   Options const& options, 
                   Stats& stats, 
                   bool useEnvelope, 
                   std::string const& collectionName,
                   std::shared_ptr<SharedState> sharedState,
                   size_t readOffset,
                   std::unique_ptr<basics::StringBuffer> buffer);
    
    Result run(arangodb::httpclient::SimpleHttpClient& client) override;
    
    size_t const readOffset;
    std::unique_ptr<basics::StringBuffer> buffer;
  };
  
  ClientTaskQueue<RestoreJob>& taskQueue();

#ifndef ARANGODB_USE_GOOGLE_TESTS
 private:
#else
 public:
#endif
  static void sortCollectionsForCreation(std::vector<VPackBuilder>& collections);

 private:
  struct DatabaseInfo {
    std::string directory;
    VPackBuilder properties;
    std::string name;
  };

  std::vector<DatabaseInfo> determineDatabaseList(std::string const& databaseName);

  ClientManager _clientManager;
  ClientTaskQueue<RestoreJob> _clientTaskQueue;
  std::unique_ptr<ManagedDirectory> _directory;
  std::unique_ptr<RestoreProgressTracker> _progressTracker;
  int& _exitCode;
  Options _options;
  Stats _stats;
  Mutex mutable _workerErrorLock;
  std::queue<Result> _workerErrors;

  Mutex _buffersLock;
  std::vector<std::unique_ptr<basics::StringBuffer>> _buffers;

};

}  // namespace arangodb

