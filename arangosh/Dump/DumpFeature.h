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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_DUMP_DUMP_FEATURE_H
#define ARANGODB_DUMP_DUMP_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/Mutex.h"
#include "Maskings/Maskings.h"
#include "Utils/ClientManager.h"
#include "Utils/ClientTaskQueue.h"
#include "Utils/ManagedDirectory.h"

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}

class DumpFeature : public application_features::ApplicationFeature {
 public:
  DumpFeature(application_features::ApplicationServer& server, int& exitCode);

  // for documentation of virtual methods, see `ApplicationFeature`
  virtual void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  virtual void validateOptions(std::shared_ptr<options::ProgramOptions> options) override final;
  virtual void start() override final;

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
  };

  /// @brief Stores stats about the overall dump progress
  struct Stats {
    std::atomic<uint64_t> totalBatches{0};
    std::atomic<uint64_t> totalCollections{0};
    std::atomic<uint64_t> totalWritten{0};
  };

  /// @brief Stores all necessary data to dump a single collection or shard
  struct JobData {
    JobData(ManagedDirectory&, DumpFeature&, Options const&,
            maskings::Maskings* maskings, Stats&, VPackSlice const&, uint64_t const,
            std::string const&, std::string const&, std::string const&);

    ManagedDirectory& directory;
    DumpFeature& feature;
    Options const& options;
    maskings::Maskings* maskings;
    Stats& stats;

    VPackSlice const collectionInfo;
    uint64_t const batchId;
    std::string const cid;
    std::string const name;
    std::string const type;
  };

 private:
  ClientManager _clientManager;
  ClientTaskQueue<JobData> _clientTaskQueue;
  std::unique_ptr<ManagedDirectory> _directory;
  int& _exitCode;
  Options _options;
  Stats _stats;
  Mutex _workerErrorLock;
  std::queue<Result> _workerErrors;
  std::unique_ptr<maskings::Maskings> _maskings;

  Result runDump(httpclient::SimpleHttpClient& client, std::string const& dbName);
  Result runClusterDump(httpclient::SimpleHttpClient& client, std::string const& dbName);
  Result storeDumpJson(VPackSlice const& body, std::string const& dbName) const;
  Result storeViews(velocypack::Slice const&) const;
};

}  // namespace arangodb

#endif
