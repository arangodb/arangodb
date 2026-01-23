////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
#include <cstdint>
#include <string>

#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

namespace arangodb {

struct RocksDBOptionFeatureOptions {
  uint64_t transactionLockStripes = 16;
  int64_t transactionLockTimeout = 1000;
  std::string walDirectory;
  uint64_t totalWriteBufferSize = 0;
  uint64_t writeBufferSize = 64 * 1024 * 1024;
  uint64_t maxWriteBufferNumber =
      RocksDBColumnFamilyManager::numberOfColumnFamilies + 2;
  int64_t maxWriteBufferSizeToMaintain = 0;
  uint64_t maxTotalWalSize = 80 << 20;
  uint64_t delayedWriteRate = 16 * 1024 * 1024;
  uint64_t minWriteBufferNumberToMerge = 1;
  uint64_t numLevels = 7;
  uint64_t numUncompressedLevels = 2;
  uint64_t maxBytesForLevelBase = 256 * 1024 * 1024;
  double maxBytesForLevelMultiplier = 10.0;
  int32_t maxBackgroundJobs = 2;
  uint32_t maxSubcompactions = 2;
  uint32_t numThreadsHigh = 0;
  uint32_t numThreadsLow = 0;
  uint64_t targetFileSizeBase = 64 * 1024 * 1024;
  uint64_t targetFileSizeMultiplier = 1;
  uint64_t blockCacheSize = 0;
  int64_t blockCacheShardBits = -1;
  uint64_t blockCacheEstimatedEntryCharge = 0;
  uint64_t minBlobSize = 256;
  uint64_t blobFileSize = 1ULL << 30;
  uint32_t blobFileStartingLevel = 0;
  bool enableBlobFiles = false;
  bool enableBlobCache = false;
  double blobGarbageCollectionAgeCutoff = 0.25;
  double blobGarbageCollectionForceThreshold = 1.0;
  double bloomBitsPerKey = 10.0;
  uint64_t tableBlockSize = 16 * 1024;
  uint64_t compactionReadaheadSize = 2 * 1024 * 1024;
  int64_t level0CompactionTrigger = 2;
  int64_t level0SlowdownTrigger = 16;
  int64_t level0StopTrigger = 256;
  uint64_t pendingCompactionBytesSlowdownTrigger = 1024ULL * 1024ULL * 1024ULL;
  uint64_t pendingCompactionBytesStopTrigger =
      32ULL * 1024ULL * 1024ULL * 1024ULL;
  uint64_t periodicCompactionTtl = 24 * 60 * 60;
  size_t recycleLogFileNum = 0;
  std::string compressionType = "lz4";
  std::string blobCompressionType = "lz4";
  std::string blockCacheType = "lru";
  std::string checksumType = "xxHash64";
  std::string compactionStyle = "level";
  uint32_t formatVersion = 5;
  bool optimizeFiltersForMemory = false;
  bool enableIndexCompression = true;
  bool useJemallocAllocator = false;
  bool prepopulateBlockCache = false;
  bool prepopulateBlobCache = false;
  bool reserveTableBuilderMemory = true;
  bool reserveTableReaderMemory = true;
  bool reserveFileMetadataMemory = true;
  bool enforceBlockCacheSizeLimit = false;
  bool cacheIndexAndFilterBlocks = true;
  bool cacheIndexAndFilterBlocksWithHighPriority = true;
  bool pinl0FilterAndIndexBlocksInCache = false;
  bool pinTopLevelIndexAndFilter = true;
  bool blockAlignDataBlocks = false;
  bool enablePipelinedWrite = true;
  bool optimizeFiltersForHits = false;
  bool useDirectReads = false;
  bool useDirectIoForFlushAndCompaction = false;
  bool useFSync = false;
  bool skipCorrupted = false;
  bool dynamicLevelBytes = true;
  bool enableStatistics = false;
  bool useFileLogging = false;
  bool limitOpenFilesAtStartup = false;
  bool allowFAllocate = true;
  bool enableBlobGarbageCollection = true;
  bool exclusiveWrites = false;
  bool minWriteBufferNumberToMergeTouched = false;
  bool partitionFilesForDocumentsCf = false;
  bool partitionFilesForPrimaryIndexCf = false;
  bool partitionFilesForEdgeIndexCf = false;
  bool partitionFilesForVPackIndexCf = false;
  bool partitionFilesForMdiIndexCf = false;
  bool partitionFilesForVectorIndexCf = false;

  std::array<uint64_t, RocksDBColumnFamilyManager::numberOfColumnFamilies>
      maxWriteBufferNumberCf = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

}  // namespace arangodb
