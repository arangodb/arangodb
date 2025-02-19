////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Dump/arangodump.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/BoundedChannel.h"
#include "Basics/Result.h"
#include "Maskings/Maskings.h"
#include "Utils/ClientManager.h"
#include "Utils/ClientTaskQueue.h"
#include "Utils/ManagedDirectory.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace arangodb {
namespace httpclient {
class SimpleHttpClient;
}

class DumpFeature final : public ArangoDumpFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Dump"; }

  DumpFeature(Server& server, int& exitCode);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;

  /**
   * @brief Saves a worker error for later handling and clears queued jobs
   * @param error Error from a client worker
   */
  void reportError(Result const& error);

  /// @brief Holds configuration data to pass between methods
  struct Options {
    std::vector<std::string> collections{};
    // Collections in here, will be ignored during the dump
    std::vector<std::string> collectionsToBeIgnored{};
    std::vector<std::string> shards{};
    std::string outputPath{};
    std::string maskingsFile{};
    uint64_t docsPerBatch{1000 * 10};
    uint64_t initialChunkSize{1024 * 1024 * 8};
    uint64_t maxChunkSize{1024 * 1024 * 64};
    // actual default value depends on the number of available cores
    uint32_t threadCount{2};
    bool allDatabases{false};
    bool clusterMode{false};
    bool dumpData{true};
    bool dumpViews{true};
    bool force{false};
    bool ignoreDistributeShardsLikeErrors{false};
    bool includeSystemCollections{false};
    bool overwrite{false};
    bool progress{true};
    bool useGzipForStorage{true};
    bool useVPack{false};
    bool useParalleDump{true};
    bool splitFiles{false};
    std::uint64_t dbserverWorkerThreads{5};
    std::uint64_t dbserverPrefetchBatches{5};
    std::uint64_t localWriterThreads{5};
    std::uint64_t localNetworkThreads{4};
  };

  /// @brief Stores stats about the overall dump progress
  struct Stats {
    std::atomic<uint64_t> totalBatches{0};
    std::atomic<uint64_t> totalReceived{0};
    std::atomic<uint64_t> totalCollections{0};
    std::atomic<uint64_t> totalWritten{0};
  };

  /// @brief base class for dump jobs
  struct DumpJob {
    DumpJob(ManagedDirectory&, DumpFeature&, Options const& options,
            maskings::Maskings* maskings, Stats& stats,
            VPackSlice collectionInfo);
    virtual ~DumpJob();

    virtual Result run(arangodb::httpclient::SimpleHttpClient& client) = 0;

    ManagedDirectory& directory;
    DumpFeature& feature;
    Options const& options;
    maskings::Maskings* maskings;
    Stats& stats;
    VPackSlice collectionInfo;
    std::string collectionName;
  };

  /// @brief stores all necessary data to dump a single collection.
  /// in cluster, this job itself will dispatch one DumpShardJob per
  /// shard of the collection!
  /// used in both single server and cluster mode
  struct DumpCollectionJob : public DumpJob {
    DumpCollectionJob(ManagedDirectory&, DumpFeature&, Options const& options,
                      maskings::Maskings* maskings, Stats& stats,
                      VPackSlice collectionInfo, uint64_t batchId);

    ~DumpCollectionJob();

    Result run(arangodb::httpclient::SimpleHttpClient& client) override;

    uint64_t const batchId;
  };

  /// @brief stores all necessary data to dump a single shard.
  /// only used in cluster mode
  struct DumpShardJob : public DumpJob {
    DumpShardJob(ManagedDirectory&, DumpFeature&, Options const& options,
                 maskings::Maskings* maskings, Stats& stats,
                 VPackSlice collectionInfo, std::string const& shardName,
                 std::string const& server,
                 std::shared_ptr<ManagedDirectory::File> file);

    Result run(arangodb::httpclient::SimpleHttpClient& client) override;

    std::string const shardName;
    std::string const server;
    std::shared_ptr<ManagedDirectory::File> file;
  };

  struct DumpFileProvider {
    explicit DumpFileProvider(
        ManagedDirectory& directory,
        std::map<std::string, arangodb::velocypack::Slice>& collectionInfo,
        bool splitFiles, bool useVPack);
    std::shared_ptr<ManagedDirectory::File> getFile(
        std::string const& collection);

   private:
    struct CollectionFiles {
      std::size_t count{0};
      std::shared_ptr<ManagedDirectory::File> file;
    };

    bool const _splitFiles;
    bool const _useVPack;
    std::mutex _mutex;
    std::unordered_map<std::string, CollectionFiles> _filesByCollection;
    ManagedDirectory& _directory;
    std::map<std::string, arangodb::velocypack::Slice>& _collectionInfo;
  };

  struct ParallelDumpServer : public DumpJob {
    struct ShardInfo {
      std::string collectionName;
    };

    ParallelDumpServer(ManagedDirectory&, DumpFeature&, ClientManager&,
                       Options const& options, maskings::Maskings* maskings,
                       Stats& stats, std::shared_ptr<DumpFileProvider>,
                       std::unordered_map<std::string, ShardInfo> shards,
                       std::string server);

    Result run(httpclient::SimpleHttpClient&) override;

    std::unique_ptr<httpclient::SimpleHttpResult> receiveNextBatch(
        httpclient::SimpleHttpClient&, std::uint64_t batchId,
        std::optional<std::uint64_t> lastBatch);

    void runNetworkThread(size_t threadId) noexcept;
    void runWriterThread();

    void createDumpContext(httpclient::SimpleHttpClient& client);
    void finishDumpContext(httpclient::SimpleHttpClient& client);

    ClientManager& clientManager;
    std::shared_ptr<DumpFileProvider> const fileProvider;
    std::unordered_map<std::string, ShardInfo> const shards;
    std::string const server;
    std::atomic<std::uint64_t> _batchCounter{0};
    std::string dumpId;
    BoundedChannel<arangodb::httpclient::SimpleHttpResult> queue;

    enum BlockAt { kLocalQueue = 0, kRemoteQueue = 1 };

    void countBlocker(BlockAt where, int64_t c);
    void printBlockStats();

    std::atomic<std::int64_t> blockCounter[2];
  };

  ClientTaskQueue<DumpJob>& taskQueue();

 private:
  ClientManager _clientManager;
  ClientTaskQueue<DumpJob> _clientTaskQueue;
  std::unique_ptr<ManagedDirectory> _directory;
  int& _exitCode;
  Options _options;
  Stats _stats;
  std::mutex _workerErrorLock;
  std::vector<Result> _workerErrors;
  std::unique_ptr<maskings::Maskings> _maskings;

  Result runClusterDump(httpclient::SimpleHttpClient& client,
                        std::string const& dbName);
  Result runSingleDump(httpclient::SimpleHttpClient& client,
                       std::string const& dbName);

  // called from both runClusterDump and runSingleDump
  Result runDump(httpclient::SimpleHttpClient& client,
                 std::string const& baseUrl, std::string const& dbName,
                 uint64_t batchId);

  Result storeDumpJson(VPackSlice body, std::string const& dbName) const;
  Result storeViews(velocypack::Slice views) const;
};

}  // namespace arangodb
