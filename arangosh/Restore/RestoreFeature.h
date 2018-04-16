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
class EncryptionFeature;
class ManagedDirectory;

class RestoreFeature final : public application_features::ApplicationFeature {
 public:
  // statistics
  struct Stats {
    uint64_t totalBatches;
    uint64_t totalCollections;
    uint64_t totalRead;
  };

  struct JobData {
    RestoreFeature& feature;
    ManagedDirectory& directory;
    Stats& stats;
  };

 public:
  RestoreFeature(application_features::ApplicationServer* server, int& exitCode);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;

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
 void reportError(Result const& error);

 private:
   int& _exitCode;

 private:
  ClientManager _clientManager;
  ClientTaskQueue<JobData> _clientTaskQueue;
  std::vector<std::string> _collections;
  std::string _inputDirectory;
  uint64_t _chunkSize;
  bool _includeSystemCollections;
  bool _createDatabase;
  std::string _inputPath;
  std::unique_ptr<ManagedDirectory> _directory;
  bool _forceSameDatabase;
  bool _importData;
  bool _importStructure;
  bool _progress;
  bool _overwrite;
  bool _force;
  bool _ignoreDistributeShardsLikeErrors;
  bool _clusterMode;
  uint64_t _defaultNumberOfShards;
  uint64_t _defaultReplicationFactor;
  Stats _stats;
  Mutex _workerErrorLock;
  std::queue<Result> _workerErrors;

 private:
  int tryCreateDatabase(std::string const& name);
  int sendRestoreCollection(VPackSlice const& slice, std::string const& name,
                            std::string& errorMsg);
  int sendRestoreIndexes(VPackSlice const& slice, std::string& errorMsg);
  int sendRestoreData(std::string const& cname, char const* buffer,
                      size_t bufferSize, std::string& errorMsg);
  Result checkEncryption();
  Result readDumpInfo();
  int processInputDirectory(std::string& errorMsg);
};
}  // namespace arangodb

#endif
