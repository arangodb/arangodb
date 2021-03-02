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

#ifndef ARANGODB_DUMP_DUMP_FEATURE_H
#define ARANGODB_DUMP_DUMP_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/Mutex.h"
#include "Basics/Result.h"
#include "Maskings/Maskings.h"
#include "Utils/ClientManager.h"
#include "Utils/ClientTaskQueue.h"
#include "Utils/ManagedDirectory.h"

#include <memory>
#include <string>

namespace arangodb {
namespace httpclient {
class SimpleHttpClient;
}

class DumpFeature final : public application_features::ApplicationFeature {
 public:
  DumpFeature(application_features::ApplicationServer& server, int& exitCode);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) override;
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
  
  /// @brief Holds configuration data to pass between methods
  struct Options {
    std::vector<std::string> collections{};
    std::vector<std::string> shards{};
    std::string outputPath{};
    std::string maskingsFile{};
    uint64_t initialChunkSize{1024 * 1024 * 8};
    uint64_t maxChunkSize{1024 * 1024 * 64};
    uint32_t threadCount{2};
    uint64_t tickStart{0};
    uint64_t tickEnd{0};
    bool allDatabases{false};
    bool clusterMode{false};
    bool dumpData{true};
    bool force{false};
    bool ignoreDistributeShardsLikeErrors{false};
    bool includeSystemCollections{false};
    bool overwrite{false};
    bool progress{true};
    bool useGzip{true};
    bool useEnvelope{true};
  };

  /// @brief Stores stats about the overall dump progress
  struct Stats {
    std::atomic<uint64_t> totalBatches{0};
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
    std::string collectionType;
  };

  /// @brief stores all necessary data to dump a single collection.
  /// in cluster, this job itself will dispatch one DumpShardJob per
  /// shard of the collection!
  /// used in both single server and cluster mode
  struct DumpCollectionJob : public DumpJob {
    DumpCollectionJob(ManagedDirectory&, DumpFeature&, Options const& options,
                      maskings::Maskings* maskings, Stats& stats,
                      VPackSlice collectionInfo,
                      uint64_t batchId);

    ~DumpCollectionJob();
    
    Result run(arangodb::httpclient::SimpleHttpClient& client) override;
    
    uint64_t const batchId;
  };
 
  /// @brief stores all necessary data to dump a single shard.
  /// only used in cluster mode
  struct DumpShardJob : public DumpJob {
    DumpShardJob(ManagedDirectory&, DumpFeature&, Options const& options,
                 maskings::Maskings* maskings, Stats& stats,
                 VPackSlice collectionInfo,
                 std::string const& shardName,
                 std::string const& server,
                 std::shared_ptr<ManagedDirectory::File> file);

    ~DumpShardJob();
    
    Result run(arangodb::httpclient::SimpleHttpClient& client) override;
    
    VPackSlice const collectionInfo;
    std::string const shardName;
    std::string const server;
    std::shared_ptr<ManagedDirectory::File> file;
  };

  ClientTaskQueue<DumpJob>& taskQueue();

 private:
  ClientManager _clientManager;
  ClientTaskQueue<DumpJob> _clientTaskQueue;
  std::unique_ptr<ManagedDirectory> _directory;
  int& _exitCode;
  Options _options;
  Stats _stats;
  Mutex _workerErrorLock;
  std::queue<Result> _workerErrors;
  std::unique_ptr<maskings::Maskings> _maskings;

  Result runClusterDump(httpclient::SimpleHttpClient& client, std::string const& dbName);
  Result runSingleDump(httpclient::SimpleHttpClient& client, std::string const& dbName);

  // called from both runClusterDump and runSingleDump
  Result runDump(httpclient::SimpleHttpClient& client, std::string const& baseUrl, std::string const& dbName, uint64_t batchId);

  Result storeDumpJson(VPackSlice body, std::string const& dbName) const;
  Result storeViews(velocypack::Slice const&) const;
};

}  // namespace arangodb

#endif
