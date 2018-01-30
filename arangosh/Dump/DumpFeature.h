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
#include "Utils/ClientManager.h"
#include "Utils/ClientTaskQueue.h"

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}
class DumpFeature;
class EncryptionFeature;
class ManagedDirectory;

class DumpFeature : public application_features::ApplicationFeature {
 public:
  /// @brief Stores stats about the overall dump progress
  struct Stats {
    Stats(uint64_t b, uint64_t c, uint64_t w) noexcept;
    std::atomic<uint64_t> totalBatches;
    std::atomic<uint64_t> totalCollections;
    std::atomic<uint64_t> totalWritten;
  };

  /// @brief Stores all necessary data to dump a single collection or shard
  struct JobData {
    JobData(DumpFeature&, ManagedDirectory&, Stats&, VPackSlice const&,
            bool const&, bool const&, bool const&, uint64_t const&,
            uint64_t const&, uint64_t const&, uint64_t const&, uint64_t const,
            std::string const&, std::string const&,
            std::string const&) noexcept;
    DumpFeature& feature;
    ManagedDirectory& directory;
    Stats& stats;
    VPackSlice const collectionInfo;
    bool const clusterMode;
    bool const showProgress;
    bool const dumpData;
    uint64_t const initialChunkSize;
    uint64_t const maxChunkSize;
    uint64_t const tickStart;
    uint64_t const tickEnd;
    uint64_t const batchId;
    std::string const cid;
    std::string const name;
    std::string const type;
  };

 public:
  DumpFeature(application_features::ApplicationServer* server, int& exitCode);

 public:
  // for documentation of virtual methods, see `ApplicationFeature`
  virtual void collectOptions(
      std::shared_ptr<options::ProgramOptions>) override final;
  virtual void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override final;
  virtual void start() override final;

 public:
  /**
   * @brief Returns the feature name (for registration with `ApplicationServer`)
   * @return The name of the feature
   */
  static std::string featureName();

  /**
   * @brief Saves a worker error for later handling and clears queued jobs
   * @param error Error from a client worker
   */
  void reportError(Result const& error) noexcept;

 private:
  Result runDump(httpclient::SimpleHttpClient& client, std::string& dbName);
  Result runClusterDump(httpclient::SimpleHttpClient& client);

 private:
  int& _exitCode;

 private:
  ClientManager _clientManager;
  ClientTaskQueue<JobData> _clientTaskQueue;
  std::unique_ptr<ManagedDirectory> _directory;
  Stats _stats;
  std::vector<std::string> _collections;
  Mutex _workerErrorLock;
  std::queue<Result> _workerErrors;

 private:
  bool _dumpData;
  bool _force;
  bool _ignoreDistributeShardsLikeErrors;
  bool _includeSystemCollections;
  bool _overwrite;
  bool _progress;
  bool _clusterMode;
  uint64_t _initialChunkSize;
  uint64_t _maxChunkSize;
  uint64_t _tickStart;
  uint64_t _tickEnd;
  uint32_t _threadCount;
  std::string _outputPath;
};
}  // namespace arangodb

#endif
