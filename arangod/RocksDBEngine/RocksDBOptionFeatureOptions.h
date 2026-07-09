////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

namespace arangodb {

// This name combination is a combination of already existing, and thus
// conflicting, classnames and the purpose of the feature that is being
// extracted here, thats why it leads to this double "Option" and the redundant
// "Feature" in the name. This all is temporary anyway, and should be remove it
// further steps.

struct RocksDBOptionFeatureOptions {
  uint64_t transactionLockStripes = 0;
  int64_t transactionLockTimeout = 0;
  std::string walDirectory;
  uint64_t totalWriteBufferSize = 0;
  uint64_t writeBufferSize = 0;
  uint64_t maxWriteBufferNumber = 0;
  int64_t maxWriteBufferSizeToMaintain = 0;
  uint64_t maxTotalWalSize = 0;
  uint64_t delayedWriteRate = 0;
  uint64_t minWriteBufferNumberToMerge = 0;
  uint64_t numLevels = 0;
  uint64_t numUncompressedLevels = 0;
  uint64_t maxBytesForLevelBase = 0;
  double maxBytesForLevelMultiplier = 0.0;
  int32_t maxBackgroundJobs = 0;
  uint32_t maxSubcompactions = 0;
  uint32_t numThreadsHigh = 0;
  uint32_t numThreadsLow = 0;
  uint64_t targetFileSizeBase = 0;
  uint64_t targetFileSizeMultiplier = 0;
  uint64_t blockCacheSize = 0;
  int64_t blockCacheShardBits = 0;
  uint64_t blockCacheEstimatedEntryCharge = 0;
  uint64_t minBlobSize = 0;
  uint64_t blobFileSize = 0;
  uint32_t blobFileStartingLevel = 0;
  bool enableBlobFiles = false;
  bool enableBlobCache = false;
  double blobGarbageCollectionAgeCutoff = 0.0;
  double blobGarbageCollectionForceThreshold = 0.0;
  double bloomBitsPerKey = 0.0;
  uint64_t tableBlockSize = 0;
  uint64_t compactionReadaheadSize = 0;
  int64_t level0CompactionTrigger = 0;
  int64_t level0SlowdownTrigger = 0;
  int64_t level0StopTrigger = 0;
  uint64_t pendingCompactionBytesSlowdownTrigger = 0;
  uint64_t pendingCompactionBytesStopTrigger = 0;
  uint64_t periodicCompactionTtl = 0;
  size_t recycleLogFileNum = 0;
  std::string compressionType;
  std::string blobCompressionType;
  std::string blockCacheType;
  std::string checksumType;
  std::string compactionStyle;
  uint32_t formatVersion = 6;
  bool optimizeFiltersForMemory = false;
  bool enableIndexCompression = false;
  bool useJemallocAllocator = false;
  bool prepopulateBlockCache = false;
  bool prepopulateBlobCache = false;
  bool reserveTableBuilderMemory = false;
  bool reserveTableReaderMemory = false;
  bool reserveFileMetadataMemory = false;
  bool enforceBlockCacheSizeLimit = false;
  bool cacheIndexAndFilterBlocks = false;
  bool cacheIndexAndFilterBlocksWithHighPriority = false;
  bool pinl0FilterAndIndexBlocksInCache = false;
  bool pinTopLevelIndexAndFilter = false;
  bool blockAlignDataBlocks = false;
  bool enablePipelinedWrite = false;
  bool optimizeFiltersForHits = false;
  bool useDirectReads = false;
  bool useDirectIoForFlushAndCompaction = false;
  bool useFSync = false;
  bool skipCorrupted = false;
  bool dynamicLevelBytes = false;
  bool enableStatistics = false;
  bool useFileLogging = false;
  bool limitOpenFilesAtStartup = false;
  bool allowFAllocate = false;
  bool enableBlobGarbageCollection = false;
  bool minWriteBufferNumberToMergeTouched = false;
  bool partitionFilesForDocumentsCf = false;
  bool partitionFilesForPrimaryIndexCf = false;
  bool partitionFilesForEdgeIndexCf = false;
  bool partitionFilesForVPackIndexCf = false;
  bool partitionFilesForMdiIndexCf = false;
  bool partitionFilesForVectorIndexCf = false;
  bool ioUringEnabled = true;
  std::array<uint64_t, RocksDBColumnFamilyManager::numberOfColumnFamilies>
      maxWriteBufferNumberCf{};
};

}  // namespace arangodb
