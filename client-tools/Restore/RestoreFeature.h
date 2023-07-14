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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Restore/arangorestore.h"

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

class RestoreFeature final : public ArangoRestoreFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Restore"; }

  RestoreFeature(Server& server, int& exitCode);

  // for documentation of virtual methods, see `ApplicationFeature`
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
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
    bool enableRevisionTrees{true};
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

  struct MultiFileReadOffset {
    size_t fileNo{0};
    size_t readOffset{0};

    friend auto operator<=>(MultiFileReadOffset const&,
                            MultiFileReadOffset const&) noexcept = default;

    auto operator+(size_t x) {
      return MultiFileReadOffset{fileNo, readOffset + x};
    }

    friend auto& operator<<(std::ostream& os, MultiFileReadOffset const& x) {
      return os << x.fileNo << ":" << x.readOffset;
    }
  };

  struct CollectionStatus {
    CollectionState state{UNKNOWN};
    MultiFileReadOffset bytesAcked;

    CollectionStatus();
    explicit CollectionStatus(CollectionState state, MultiFileReadOffset);
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
    std::mutex mutex;

    /// @brief this contains errors produced by background send operations
    /// (i.e. RestoreSendJobs)
    Result result;

    /// @brief data chunk offsets (start offset, length) of requests that are
    /// currently ongoing. Resumption of the restore must start at the smallest
    /// key value contained in here (note: we also need to track the length of
    /// each chunk to test the resumption of a restore after a crash)
    std::map<MultiFileReadOffset, size_t> readOffsets;

    /// @brief number of dispatched jobs that we need to wait for until we
    /// can declare final success. this is important only when restoring the
    /// data for a collection/shards from multiple files.
    size_t pendingJobs{0};

    /// @brief whether ot not we have read the complete input data file for the
    /// collection
    bool readCompleteInputfile{false};
  };

  /// @brief Stores all necessary data to restore a single collection or shard
  struct RestoreJob {
    RestoreJob(RestoreFeature& feature, RestoreProgressTracker& progressTracker,
               Options const& options, Stats& stats, bool useEnvelope,
               std::string const& collectionName,
               std::shared_ptr<SharedState> sharedState);

    virtual ~RestoreJob();

    virtual Result run(arangodb::httpclient::SimpleHttpClient& client) = 0;

    void updateProgress();

    Result sendRestoreData(arangodb::httpclient::SimpleHttpClient& client,
                           MultiFileReadOffset readOffset, char const* buffer,
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
    RestoreMainJob(ManagedDirectory& directory, RestoreFeature& feature,
                   RestoreProgressTracker& progressTracker,
                   Options const& options, Stats& stats, VPackSlice parameters,
                   bool useEnvelope);

    Result run(arangodb::httpclient::SimpleHttpClient& client) override;

    Result dispatchRestoreData(arangodb::httpclient::SimpleHttpClient& client,
                               MultiFileReadOffset readOffset, char const* data,
                               size_t length, bool forceDirect);

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
                   Options const& options, Stats& stats, bool useEnvelope,
                   std::string const& collectionName,
                   std::shared_ptr<SharedState> sharedState,
                   MultiFileReadOffset readOffset,
                   std::unique_ptr<basics::StringBuffer> buffer);

    ~RestoreSendJob();

    Result run(arangodb::httpclient::SimpleHttpClient& client) override;

    MultiFileReadOffset const readOffset;
    std::unique_ptr<basics::StringBuffer> buffer;
  };

  ClientTaskQueue<RestoreJob>& taskQueue();

  static void sortCollectionsForCreation(
      std::vector<VPackBuilder>& collections);

 private:
  struct DatabaseInfo {
    std::string directory;
    VPackBuilder properties;
    std::string name;
  };

  std::vector<DatabaseInfo> determineDatabaseList(
      std::string const& databaseName);

  ClientManager _clientManager;
  ClientTaskQueue<RestoreJob> _clientTaskQueue;
  std::unique_ptr<ManagedDirectory> _directory;
  std::unique_ptr<RestoreProgressTracker> _progressTracker;
  int& _exitCode;
  Options _options;
  Stats _stats;
  std::mutex mutable _workerErrorLock;
  std::vector<Result> _workerErrors;

  std::mutex _buffersLock;
  std::vector<std::unique_ptr<basics::StringBuffer>> _buffers;
};

}  // namespace arangodb
