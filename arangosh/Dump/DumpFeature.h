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

#ifndef ARANGODB_DUMP_DUMP_FEATURE_H
#define ARANGODB_DUMP_DUMP_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "V8Client/ArangoClientHelper.h"

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}

class DumpFeature final : public application_features::ApplicationFeature,
                                public ArangoClientHelper {
 public:
  DumpFeature(application_features::ApplicationServer* server,
                    int* result);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override final;
  void prepare() override final;
  void start() override final;

 private:
  std::vector<std::string> _collections;
  uint64_t _chunkSize;
  uint64_t _maxChunkSize;
  bool _dumpData;
  bool _force;
  bool _includeSystemCollections;
  std::string _outputDirectory;
  bool _overwrite;
  bool _progress;
  uint64_t _tickStart;
  uint64_t _tickEnd;
  bool _compat28;

 private:
  int startBatch(std::string DBserver, std::string& errorMsg);
  void extendBatch(std::string DBserver);
  void endBatch(std::string DBserver);
  int dumpCollection(int fd, std::string const& cid, std::string const& name,
                     uint64_t maxTick, std::string& errorMsg);
  void flushWal();
  int runDump(std::string& dbName, std::string& errorMsg);
  int dumpShard(int fd, std::string const& DBserver, std::string const& name,
                std::string& errorMsg);
  int runClusterDump(std::string& errorMsg);

 private:
  int* _result;

  // our batch id
  uint64_t _batchId;

  // cluster mode flag
  bool _clusterMode;

  // statistics
  struct {
    uint64_t _totalBatches;
    uint64_t _totalCollections;
    uint64_t _totalWritten;
  } _stats;
};
}

#endif
