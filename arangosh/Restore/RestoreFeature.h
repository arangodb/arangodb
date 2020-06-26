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

#ifndef ARANGODB_RESTORE_RESTORE_FEATURE_H
#define ARANGODB_RESTORE_RESTORE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/VelocyPackHelper.h"
#include "Utils/ClientManager.h"
#include "Utils/ClientTaskQueue.h"
#include "Utils/ManagedDirectory.h"

namespace arangodb {

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
    bool cleanupDuplicateAttributes{false};
    bool progress{true};
    bool preserveRevisionIds{true};
  };

  /// @brief Stores stats about the overall restore progress
  struct Stats {
    std::atomic<uint64_t> totalBatches{0};
    std::atomic<uint64_t> totalSent{0};
    std::atomic<uint64_t> totalCollections{0};
    std::atomic<uint64_t> restoredCollections{0};
    std::atomic<uint64_t> totalRead{0};
  };

  /// @brief Stores all necessary data to restore a single collection or shard
  struct JobData {
    ManagedDirectory& directory;
    RestoreFeature& feature;
    Options const& options;
    Stats& stats;

    VPackSlice collection;

    JobData(ManagedDirectory&, RestoreFeature&, Options const&, Stats&, VPackSlice const&);
  };

 private:
  ClientManager _clientManager;
  ClientTaskQueue<JobData> _clientTaskQueue;
  std::unique_ptr<ManagedDirectory> _directory;
  int& _exitCode;
  Options _options;
  Stats _stats;
  Mutex mutable _workerErrorLock;
  std::queue<Result> _workerErrors;
};

}  // namespace arangodb

#endif
