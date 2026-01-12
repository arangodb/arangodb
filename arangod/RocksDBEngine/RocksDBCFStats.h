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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <rocksdb/db.h>
#include <rocksdb/cache.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace arangodb {

/// @brief Statistics for a single compaction level
struct CompactionLevelStats {
  std::string level;  // Level name (e.g., "L0", "L1", "Sum")
  uint64_t numFiles{0};
  uint64_t compactedFiles{0};
  double sizeBytes{0.0};
  double score{0.0};
  double readGB{0.0};
  double rnGB{0.0};
  double rnp1GB{0.0};
  double writeGB{0.0};
  double wnewGB{0.0};
  double movedGB{0.0};
  double writeAmp{0.0};
  double readMBps{0.0};
  double writeMBps{0.0};
  double compSec{0.0};
  double compMergeCPUSec{0.0};
  uint64_t compCount{0};
  double avgSec{0.0};
  uint64_t keyIn{0};
  uint64_t keyDrop{0};
  double rblobGB{0.0};
  double wblobGB{0.0};

  template<class Inspector>
  friend auto inspect(Inspector& f, CompactionLevelStats& x) {
    return f.object(x).fields(
        f.field("level", x.level), f.field("numFiles", x.numFiles),
        f.field("compactedFiles", x.compactedFiles),
        f.field("sizeBytes", x.sizeBytes), f.field("score", x.score),
        f.field("readGB", x.readGB), f.field("rnGB", x.rnGB),
        f.field("rnp1GB", x.rnp1GB), f.field("writeGB", x.writeGB),
        f.field("wnewGB", x.wnewGB), f.field("movedGB", x.movedGB),
        f.field("writeAmp", x.writeAmp), f.field("readMBps", x.readMBps),
        f.field("writeMBps", x.writeMBps), f.field("compSec", x.compSec),
        f.field("compMergeCPUSec", x.compMergeCPUSec),
        f.field("compCount", x.compCount), f.field("avgSec", x.avgSec),
        f.field("keyIn", x.keyIn), f.field("keyDrop", x.keyDrop),
        f.field("rblobGB", x.rblobGB), f.field("wblobGB", x.wblobGB));
  }
};

/// @brief Write stall statistics
struct WriteStallStats {
  uint64_t cfL0FileCountLimitDelaysWithOngoingCompaction{0};
  uint64_t cfL0FileCountLimitStopsWithOngoingCompaction{0};
  uint64_t l0FileCountLimitDelays{0};
  uint64_t l0FileCountLimitStops{0};
  uint64_t memtableLimitDelays{0};
  uint64_t memtableLimitStops{0};
  uint64_t pendingCompactionBytesDelays{0};
  uint64_t pendingCompactionBytesStops{0};
  uint64_t totalDelays{0};
  uint64_t totalStops{0};

  template<class Inspector>
  friend auto inspect(Inspector& f, WriteStallStats& x) {
    return f.object(x).fields(
        f.field("cfL0FileCountLimitDelaysWithOngoingCompaction",
                x.cfL0FileCountLimitDelaysWithOngoingCompaction),
        f.field("cfL0FileCountLimitStopsWithOngoingCompaction",
                x.cfL0FileCountLimitStopsWithOngoingCompaction),
        f.field("l0FileCountLimitDelays", x.l0FileCountLimitDelays),
        f.field("l0FileCountLimitStops", x.l0FileCountLimitStops),
        f.field("memtableLimitDelays", x.memtableLimitDelays),
        f.field("memtableLimitStops", x.memtableLimitStops),
        f.field("pendingCompactionBytesDelays", x.pendingCompactionBytesDelays),
        f.field("pendingCompactionBytesStops", x.pendingCompactionBytesStops),
        f.field("totalDelays", x.totalDelays),
        f.field("totalStops", x.totalStops));
  }
};

/// @brief Block cache entry statistics for a specific role
struct BlockCacheRoleStats {
  uint64_t count{0};
  uint64_t usedBytes{0};
  double usedPercent{0.0};

  template<class Inspector>
  friend auto inspect(Inspector& f, BlockCacheRoleStats& x) {
    return f.object(x).fields(f.field("count", x.count),
                              f.field("usedBytes", x.usedBytes),
                              f.field("usedPercent", x.usedPercent));
  }
};

/// @brief Block cache statistics
struct BlockCacheStats {
  std::string cacheId;
  uint64_t capacityBytes{0};
  double lastCollectionDurationSecs{0.0};
  uint64_t lastCollectionAgeSecs{0};

  // Stats per cache entry role
  std::map<std::string, BlockCacheRoleStats> roleStats;

  template<class Inspector>
  friend auto inspect(Inspector& f, BlockCacheStats& x) {
    return f.object(x).fields(
        f.field("cacheId", x.cacheId),
        f.field("capacityBytes", x.capacityBytes),
        f.field("lastCollectionDurationSecs", x.lastCollectionDurationSecs),
        f.field("lastCollectionAgeSecs", x.lastCollectionAgeSecs),
        f.field("roleStats", x.roleStats));
  }
};

/// @brief Complete statistics for a column family
struct ColumnFamilyStats {
  std::string columnFamilyName;

  // Compaction stats by level (L0, L1, ... and Sum)
  std::vector<CompactionLevelStats> levelStats;
  std::optional<CompactionLevelStats> sumStats;

  // Integer properties from GetIntProperty
  uint64_t numImmutableMemTable{0};
  uint64_t numImmutableMemTableFlushed{0};
  uint64_t memTableFlushPending{0};
  uint64_t compactionPending{0};
  uint64_t curSizeActiveMemTable{0};
  uint64_t curSizeAllMemTables{0};
  uint64_t sizeAllMemTables{0};
  uint64_t numEntriesActiveMemTable{0};
  uint64_t numEntriesImmMemTables{0};
  uint64_t numDeletesActiveMemTable{0};
  uint64_t numDeletesImmMemTables{0};
  uint64_t estimateNumKeys{0};
  uint64_t estimateTableReadersMem{0};
  uint64_t numLiveVersions{0};
  uint64_t estimateLiveDataSize{0};
  uint64_t liveSstFilesSize{0};
  uint64_t estimatePendingCompactionBytes{0};

  // Blob file stats
  uint64_t numBlobFiles{0};
  uint64_t liveBlobFileSize{0};
  uint64_t liveBlobFileGarbageSize{0};

  // Write stall stats
  WriteStallStats writeStallStats;

  // Approximate memory usage (size on disk and in memtables)
  uint64_t memory{0};

  template<class Inspector>
  friend auto inspect(Inspector& f, ColumnFamilyStats& x) {
    return f.object(x).fields(
        f.field("columnFamilyName", x.columnFamilyName),
        f.field("levelStats", x.levelStats), f.field("sumStats", x.sumStats),
        f.field("numImmutableMemTable", x.numImmutableMemTable),
        f.field("numImmutableMemTableFlushed", x.numImmutableMemTableFlushed),
        f.field("memTableFlushPending", x.memTableFlushPending),
        f.field("compactionPending", x.compactionPending),
        f.field("curSizeActiveMemTable", x.curSizeActiveMemTable),
        f.field("curSizeAllMemTables", x.curSizeAllMemTables),
        f.field("sizeAllMemTables", x.sizeAllMemTables),
        f.field("numEntriesActiveMemTable", x.numEntriesActiveMemTable),
        f.field("numEntriesImmMemTables", x.numEntriesImmMemTables),
        f.field("numDeletesActiveMemTable", x.numDeletesActiveMemTable),
        f.field("numDeletesImmMemTables", x.numDeletesImmMemTables),
        f.field("estimateNumKeys", x.estimateNumKeys),
        f.field("estimateTableReadersMem", x.estimateTableReadersMem),
        f.field("numLiveVersions", x.numLiveVersions),
        f.field("estimateLiveDataSize", x.estimateLiveDataSize),
        f.field("liveSstFilesSize", x.liveSstFilesSize),
        f.field("estimatePendingCompactionBytes",
                x.estimatePendingCompactionBytes),
        f.field("numBlobFiles", x.numBlobFiles),
        f.field("liveBlobFileSize", x.liveBlobFileSize),
        f.field("liveBlobFileGarbageSize", x.liveBlobFileGarbageSize),
        f.field("writeStallStats", x.writeStallStats),
        f.field("memory", x.memory));
  }
};

/// @brief Database-wide statistics
struct DatabaseStats {
  // From GetIntProperty (DB-wide)
  uint64_t numSnapshots{0};
  uint64_t oldestSnapshotTime{0};
  uint64_t numRunningCompactions{0};
  uint64_t numRunningFlushes{0};
  uint64_t blockCacheCapacity{0};
  uint64_t blockCacheUsage{0};
  uint64_t blockCachePinnedUsage{0};

  // Block cache entry stats
  std::optional<BlockCacheStats> blockCacheStats;

  template<class Inspector>
  friend auto inspect(Inspector& f, DatabaseStats& x) {
    return f.object(x).fields(
        f.field("numSnapshots", x.numSnapshots),
        f.field("oldestSnapshotTime", x.oldestSnapshotTime),
        f.field("numRunningCompactions", x.numRunningCompactions),
        f.field("numRunningFlushes", x.numRunningFlushes),
        f.field("blockCacheCapacity", x.blockCacheCapacity),
        f.field("blockCacheUsage", x.blockCacheUsage),
        f.field("blockCachePinnedUsage", x.blockCachePinnedUsage),
        f.field("blockCacheStats", x.blockCacheStats));
  }
};

/// @brief Collector for RocksDB column family statistics using direct APIs
class RocksDBCFStatsCollector {
 public:
  /// @brief Collect statistics for a specific column family
  /// @param db The RocksDB database instance
  /// @param cfHandle The column family handle
  /// @param cfName The name of the column family
  /// @return Collected ColumnFamilyStats structure
  static ColumnFamilyStats collect(rocksdb::DB* db,
                                   rocksdb::ColumnFamilyHandle* cfHandle,
                                   std::string const& cfName);

  /// @brief Collect database-wide statistics
  /// @param db The RocksDB database instance
  /// @return Collected DatabaseStats structure
  static DatabaseStats collectDatabaseStats(rocksdb::DB* db);

  /// @brief Collect block cache statistics
  /// @param db The RocksDB database instance
  /// @return Collected BlockCacheStats structure (if available)
  static std::optional<BlockCacheStats> collectBlockCacheStats(rocksdb::DB* db);

 private:
  /// @brief Parse compaction stats from the CF stats map
  static void parseCompactionStats(
      std::map<std::string, std::string> const& cfStatsMap,
      ColumnFamilyStats& stats);

  /// @brief Parse write stall stats from the write stall map
  static void parseWriteStallStats(
      std::map<std::string, std::string> const& writeStallMap,
      WriteStallStats& stats);

  /// @brief Parse block cache entry stats from the map
  static void parseBlockCacheEntryStats(
      std::map<std::string, std::string> const& cacheStatsMap,
      BlockCacheStats& stats);

  /// @brief Get a uint64 property from DB
  static uint64_t getIntProperty(rocksdb::DB* db,
                                 rocksdb::ColumnFamilyHandle* cfHandle,
                                 std::string const& property);

  /// @brief Get a uint64 property from DB (default column family)
  static uint64_t getIntProperty(rocksdb::DB* db, std::string const& property);
};

}  // namespace arangodb
