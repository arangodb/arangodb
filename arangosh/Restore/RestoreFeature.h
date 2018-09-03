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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_RESTORE_RESTORE_FEATURE_H
#define ARANGODB_RESTORE_RESTORE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/VelocyPackHelper.h"
#include "Shell/ClientFeature.h"
#include "V8Client/ArangoClientHelper.h"

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}

#ifdef USE_ENTERPRISE
class EncryptionFeature;
#endif

class RestoreFeature final : public application_features::ApplicationFeature,
                             public ArangoClientHelper {
 public:
  RestoreFeature(application_features::ApplicationServer* server, int* result);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;

 private:
  std::vector<std::string> _collections;
  std::string _inputDirectory;
  uint64_t _chunkSize;
  bool _includeSystemCollections;
  bool _createDatabase;
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

 private:
  int tryCreateDatabase(ClientFeature*, std::string const& name);
  int sendRestoreCollection(VPackSlice const& slice, std::string const& name,
                            std::string& errorMsg);
  int sendRestoreIndexes(VPackSlice const& slice, std::string& errorMsg);
  int sendRestoreData(std::string const& cname, char const* buffer,
                      size_t bufferSize, std::string& errorMsg);
  Result readEncryptionInfo();
  Result readDumpInfo();
  int processInputDirectory(std::string& errorMsg);
  ssize_t readData(int fd, char* data, size_t len);
  void beginDecryption(int fd);
  void endDecryption(int fd);

 private:
  int* _result;
#ifdef USE_ENTERPRISE
  EncryptionFeature* _encryption;
#endif

  // statistics
  struct {
    uint64_t _totalBatches;
    uint64_t _totalSent;
    uint64_t _totalCollections;
    uint64_t _totalRead;
  } _stats;
};
}

#endif
