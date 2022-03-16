////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "RocksDBEngine/RocksDBOptionsProvider.h"

namespace arangodb::sepp {

struct RocksDBOptions : arangodb::RocksDBOptionsProvider {
  RocksDBOptions();

  rocksdb::TransactionDBOptions getTransactionDBOptions() const override;
  rocksdb::Options getOptions() const override;
  rocksdb::BlockBasedTableOptions getTableOptions() const override;

  uint64_t maxTotalWalSize() const noexcept override;
  uint32_t numThreadsHigh() const noexcept override;
  uint32_t numThreadsLow() const noexcept override;

  // template<class Inspector>
  // inline friend auto inspect(Inspector& f, RocksDBOptions& o) {
  //   return f.object(o).fields(
  //       f.field("writeBufferSize", o._writeBufferSize),
  //       f.field("totalWriteBufferSize", o._totalWriteBufferSize));
  // }

  struct GeneralOptions {
    uint32_t numThreadsLow;
    uint32_t numThreadsHigh;

    uint64_t maxTotalWalSize;

    bool allowFAllocate;
    bool enablePipelinedWrite;
    uint64_t writeBufferSize;
    uint32_t maxWriteBufferNumber;
    uint32_t maxWriteBufferNumberToMaintain = 1;
    int64_t maxWriteBufferSizeToMaintain;
    uint64_t delayedWriteRate;
    uint64_t minWriteBufferNumberToMerge;
    uint64_t numLevels;
    bool levelCompactionDynamicLevelBytes;
    uint64_t maxBytesForLevelBase;
    double maxBytesForLevelMultiplier;
    bool optimizeFiltersForHits;
    bool useDirectReads;
    bool useDirectIoForFlushAndCompaction;

    uint64_t targetFileSizeBase;
    uint64_t targetFileSizeMultiplier;

    int32_t maxBackgroundJobs;
    uint32_t maxSubcompactions;
    bool useFSync;

    uint32_t numUncompressedLevels;

    // TODO - compression algo

    // Number of files to trigger level-0 compaction. A value <0 means that
    // level-0 compaction will not be triggered by number of files at all.
    // Default: 4
    int64_t level0FileNumCompactionTrigger;

    // Soft limit on number of level-0 files. We start slowing down writes at
    // this point. A value <0 means that no writing slow down will be triggered
    // by number of files in level-0.
    int64_t level0SlowdownWritesTrigger;

    // Maximum number of level-0 files.  We stop writes at this point.
    int64_t level0StopWritesTrigger;

    // Soft limit on pending compaction bytes. We start slowing down writes
    // at this point.
    uint64_t pendingCompactionBytesSlowdownTrigger;

    // Maximum number of pending compaction bytes. We stop writes at this point.
    uint64_t pendingCompactionBytesStopTrigger;

    size_t recycleLogFileNum;
    uint64_t compactionReadaheadSize;

    bool enableStatistics;

    uint64_t totalWriteBufferSize;

    double memtablePrefixBloomSizeRatio = 0.2;
    // TODO: memtable_insert_with_hint_prefix_extractor?
    uint32_t bloomLocality = 1;
  };

  struct DBOptions {
    uint32_t numStripes;
    int64_t transactionLockTimeout;
  };

  struct TableOptions {
    // blockCache;
    //    uint64_t _blockCacheSize;
    //    int64_t _blockCacheShardBits;
    //    bool _enforceBlockCacheSizeLimit;
    bool cacheIndexAndFilterBlocks;
    bool cacheIndexAndFilterBlocksWithHighPriority;
    bool pinl0FilterAndIndexBlocksInCache;
    bool pinTopLevelIndexAndFilter;

    uint64_t blockSize;
    // filterPolicy;

    uint32_t formatVersion;
    bool blockAlignDataBlocks;
    rocksdb::ChecksumType checksum;
  };

 private:
  DBOptions _dbOptions;
  TableOptions _tableOptions;
  GeneralOptions _options;
};

}  // namespace arangodb::sepp
