////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_APPLICATION_FEATURES_ROCKSDB_OPTION_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_ROCKSDB_OPTION_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Common.h"
#include "VocBase/voc-types.h"

namespace arangodb {

// This feature is used to configure RocksDB in a central place.
//
// The RocksDB-Storage-Engine and the MMFiles-Persistent-Index
// that are never activated at the same time take options set
// in this feature

class RocksDBOptionFeature final
    : public application_features::ApplicationFeature {
 public:
  explicit RocksDBOptionFeature(
      application_features::ApplicationServer* server);
  ~RocksDBOptionFeature() {}

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

  int64_t _transactionLockTimeout;
  std::string _walDirectory;
  uint64_t _writeBufferSize;
  uint64_t _maxWriteBufferNumber;
  uint64_t _maxTotalWalSize;
  uint64_t _delayedWriteRate;
  uint64_t _minWriteBufferNumberToMerge;
  uint64_t _numLevels;
  uint64_t _numUncompressedLevels;
  uint64_t _maxBytesForLevelBase;
  double _maxBytesForLevelMultiplier;
  int32_t _maxBackgroundJobs;
  uint64_t _maxSubcompactions;
  uint32_t _numThreadsHigh;
  uint32_t _numThreadsLow;
  uint64_t _blockCacheSize;
  uint64_t _blockCacheShardBits;
  uint64_t _tableBlockSize;
  uint64_t _recycleLogFileNum;
  uint64_t _compactionReadaheadSize;
  int64_t _level0CompactionTrigger;
  int64_t _level0SlowdownTrigger;
  int64_t _level0StopTrigger;
  bool _enablePipelinedWrite;
  bool _optimizeFiltersForHits;
  bool _useDirectReads;
  bool _useDirectIoForFlushAndCompaction;
  bool _useFSync;
  bool _skipCorrupted;
  bool _dynamicLevelBytes;
  bool _enableStatistics;
};

}  // namespace arangodb

#endif
