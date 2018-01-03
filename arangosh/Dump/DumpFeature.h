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
#include "Utils/ClientManager.hpp"
#include "Utils/ClientTaskQueue.hpp"
#include "Utils/FileHandler.hpp"

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}
class DumpFeature;
class EncryptionFeature;

struct DumpFeatureStats {
  DumpFeatureStats(uint64_t b, uint64_t c, uint64_t w) noexcept;
  std::atomic<uint64_t> totalBatches;
  std::atomic<uint64_t> totalCollections;
  std::atomic<uint64_t> totalWritten;
};

struct DumpFeatureJobData {
  DumpFeatureJobData(DumpFeature&, FileHandler& filehandler, VPackSlice const&,
                     std::string const&, std::string const&, std::string const&,
                     uint64_t const&, uint64_t const&, uint64_t const&,
                     uint64_t const&, uint64_t const&, bool const&, bool const&,
                     bool const&, std::string const&,
                     DumpFeatureStats&) noexcept;
  DumpFeature& feature;
  FileHandler& fileHandler;
  VPackSlice const& collectionInfo;
  std::string const cid;
  std::string const name;
  std::string const type;
  uint64_t batchId;
  uint64_t const tickStart;
  uint64_t const maxTick;
  uint64_t const initialChunkSize;
  uint64_t const maxChunkSize;
  bool const clusterMode;
  bool const showProgress;
  bool const dumpData;
  std::string const& outputDirectory;
  DumpFeatureStats& stats;
};
extern template class ClientTaskQueue<DumpFeatureJobData>;

class DumpFeature : public application_features::ApplicationFeature {
 public:
  DumpFeature(application_features::ApplicationServer* server, int* result);

 public:
  // for documentation of virtual methods, see `ApplicationFeature`
  virtual void collectOptions(
      std::shared_ptr<options::ProgramOptions>) override final;
  virtual void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override final;
  virtual void prepare() override final;
  virtual void start() override final;

public:

  /**
   * Returns the feature name (for registration with `ApplicationServer`)
   * @return The name of the feature
   */
  static std::string featureName();

 private:
  Result runDump(std::string& dbName);
  Result runClusterDump();

 private:
  int* _result;
  ClientManager _clientManager;
  ClientTaskQueue<DumpFeatureJobData> _clientTaskQueue;
  FileHandler _fileHandler;
  DumpFeatureStats _stats;
  std::vector<std::string> _collections;
  uint64_t _chunkSize;
  uint64_t _maxChunkSize;
  bool _dumpData;
  bool _force;
  bool _ignoreDistributeShardsLikeErrors;
  bool _includeSystemCollections;
  bool _overwrite;
  bool _progress;
  bool _clusterMode;
  uint64_t _batchId;
  uint64_t _tickStart;
  uint64_t _tickEnd;
  std::string _outputDirectory;
};
}  // namespace arangodb

#endif
