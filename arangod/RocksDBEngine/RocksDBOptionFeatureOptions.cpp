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

#include "RocksDBEngine/RocksDBOptionFeatureOptions.h"

#include <algorithm>
#include <cstddef>

#include <rocksdb/options.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/transaction_db.h>

#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

namespace {

uint64_t defaultBlockCacheSize() {
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.3
    return static_cast<uint64_t>((arangodb::PhysicalMemory::getValue() -
                                  (static_cast<uint64_t>(2) << 30)) *
                                 0.3);
  }
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(2) << 30)) {
    // if we have at least 2GB of RAM, the default size is 512MB
    return (static_cast<uint64_t>(512) << 20);
  }
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(1) << 30)) {
    // if we have at least 1GB of RAM, the default size is 256MB
    return (static_cast<uint64_t>(256) << 20);
  }
  // for everything else the default size is 128MB
  return (static_cast<uint64_t>(128) << 20);
}

uint64_t defaultTotalWriteBufferSize() {
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.4
    return static_cast<uint64_t>((arangodb::PhysicalMemory::getValue() -
                                  (static_cast<uint64_t>(2) << 30)) *
                                 0.4);
  }
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(1) << 30)) {
    // if we have at least 1GB of RAM, the default size is 512MB
    return (static_cast<uint64_t>(512) << 20);
  }
  // for everything else the default size is 256MB
  return (static_cast<uint64_t>(256) << 20);
}

}  // namespace

namespace arangodb {

uint64_t defaultMinWriteBufferNumberToMerge(uint64_t rocksDBDefault,
                                            uint64_t totalSize,
                                            uint64_t sizePerBuffer,
                                            uint64_t maxBuffers) {
  uint64_t safe = rocksDBDefault;
  uint64_t test = safe + 1;

  // increase it to as much as 4 if it makes sense
  for (; test <= 4; ++test) {
    // next make sure we have enough buffers for it to matter
    uint64_t minBuffers = 1 + (2 * test);
    if (maxBuffers < minBuffers) {
      break;
    }

    // next make sure we have enough space for all the buffers
    if (minBuffers * sizePerBuffer *
            arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies >
        totalSize) {
      break;
    }

    safe = test;
  }

  return safe;
}

RocksDBOptionFeatureOptions::RocksDBOptionFeatureOptions() {
  rocksdb::TransactionDBOptions rocksDBTrxDefaults;
  rocksdb::Options rocksDBDefaults;
  rocksdb::BlockBasedTableOptions rocksDBTableOptionsDefaults;

  transactionLockStripes = std::max(NumberOfCores::getValue(), std::size_t(16));
  transactionLockTimeout = rocksDBTrxDefaults.transaction_lock_timeout;
  totalWriteBufferSize = rocksDBDefaults.db_write_buffer_size;
  writeBufferSize = rocksDBDefaults.write_buffer_size;
  maxWriteBufferNumber = RocksDBColumnFamilyManager::numberOfColumnFamilies + 2;
  maxTotalWalSize = 80 << 20;
  delayedWriteRate = rocksDBDefaults.delayed_write_rate;
  minWriteBufferNumberToMerge = defaultMinWriteBufferNumberToMerge(
      rocksDBDefaults.min_write_buffer_number_to_merge, totalWriteBufferSize,
      writeBufferSize, maxWriteBufferNumber);
  numLevels = rocksDBDefaults.num_levels;
  numUncompressedLevels = 2;
  maxBytesForLevelBase = rocksDBDefaults.max_bytes_for_level_base;
  maxBytesForLevelMultiplier = rocksDBDefaults.max_bytes_for_level_multiplier;
  maxBackgroundJobs = static_cast<int32_t>(
      std::max(static_cast<std::size_t>(2), NumberOfCores::getValue()));
  maxSubcompactions = 2;
  targetFileSizeBase = rocksDBDefaults.target_file_size_base;
  targetFileSizeMultiplier = rocksDBDefaults.target_file_size_multiplier;
  blockCacheSize = ::defaultBlockCacheSize();
  blockCacheShardBits = -1;
  minBlobSize = 256;
  blobFileSize = 1ULL << 30;
  blobGarbageCollectionAgeCutoff = 0.25;
  blobGarbageCollectionForceThreshold = 1.0;
  bloomBitsPerKey = 10.0;
  tableBlockSize = std::max(
      rocksDBTableOptionsDefaults.block_size,
      static_cast<decltype(rocksDBTableOptionsDefaults.block_size)>(16 * 1024));
  compactionReadaheadSize = 2 * 1024 * 1024;
  level0CompactionTrigger = 2;
  level0SlowdownTrigger = 16;
  level0StopTrigger = 256;
  pendingCompactionBytesSlowdownTrigger = 1024ULL * 1024ULL * 1024ULL;
  pendingCompactionBytesStopTrigger = 32ULL * 1024ULL * 1024ULL * 1024ULL;
  periodicCompactionTtl = 24 * 60 * 60;
  recycleLogFileNum = rocksDBDefaults.recycle_log_file_num;
  compressionType = std::string{kCompressionTypeLZ4};
  blobCompressionType = std::string{kCompressionTypeLZ4};
  blockCacheType = std::string{kBlockCacheTypeLRU};
  checksumType = std::string{kChecksumTypeXXHash64};
  compactionStyle = std::string{kCompactionStyleLevel};
  formatVersion = 5;
  enableIndexCompression = rocksDBTableOptionsDefaults.enable_index_compression;
  reserveTableBuilderMemory = true;
  reserveTableReaderMemory = true;
  reserveFileMetadataMemory = true;
  cacheIndexAndFilterBlocks = true;
  cacheIndexAndFilterBlocksWithHighPriority =
      rocksDBTableOptionsDefaults
          .cache_index_and_filter_blocks_with_high_priority;
  pinl0FilterAndIndexBlocksInCache =
      rocksDBTableOptionsDefaults.pin_l0_filter_and_index_blocks_in_cache;
  pinTopLevelIndexAndFilter =
      rocksDBTableOptionsDefaults.pin_top_level_index_and_filter;
  blockAlignDataBlocks = rocksDBTableOptionsDefaults.block_align;
  enablePipelinedWrite = true;
  optimizeFiltersForHits = rocksDBDefaults.optimize_filters_for_hits;
  useDirectReads = rocksDBDefaults.use_direct_reads;
  useDirectIoForFlushAndCompaction =
      rocksDBDefaults.use_direct_io_for_flush_and_compaction;
  useFSync = rocksDBDefaults.use_fsync;
  dynamicLevelBytes = true;
  allowFAllocate = true;
  enableBlobGarbageCollection = true;

  if (totalWriteBufferSize == 0) {
    totalWriteBufferSize = ::defaultTotalWriteBufferSize();
  }
}

}  // namespace arangodb
