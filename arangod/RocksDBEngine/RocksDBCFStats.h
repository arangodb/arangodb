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
/// @author Josip Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <rocksdb/db.h>
#include <rocksdb/cache.h>
#include <velocypack/Builder.h>

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

  /// @brief Serialize to VelocyPack
  void toVPack(velocypack::Builder& builder) const;
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

  /// @brief Serialize to VelocyPack
  void toVPack(velocypack::Builder& builder) const;
};

/// @brief Block cache entry statistics for a specific role
struct BlockCacheRoleStats {
  uint64_t count{0};
  uint64_t usedBytes{0};
  double usedPercent{0.0};

  /// @brief Serialize to VelocyPack
  void toVPack(velocypack::Builder& builder) const;
};

/// @brief Block cache statistics
struct BlockCacheStats {
  std::string cacheId;
  uint64_t capacityBytes{0};
  double lastCollectionDurationSecs{0.0};
  uint64_t lastCollectionAgeSecs{0};

  // Stats per cache entry role
  std::map<std::string, BlockCacheRoleStats> roleStats;

  /// @brief Serialize to VelocyPack
  void toVPack(velocypack::Builder& builder) const;
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

  /// @brief Serialize to VelocyPack (adds fields to an already-open object)
  void toVPack(velocypack::Builder& builder) const;
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

  /// @brief Serialize to VelocyPack (adds fields to an already-open object)
  void toVPack(velocypack::Builder& builder) const;
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

  /// @brief Helper: parse a double value from string
  static double parseDouble(std::string const& s);

  /// @brief Helper: parse a uint64 value from string
  static uint64_t parseUint64(std::string const& s);

  /// @brief Get a uint64 property from DB
  static uint64_t getIntProperty(rocksdb::DB* db,
                                 rocksdb::ColumnFamilyHandle* cfHandle,
                                 std::string const& property);

  /// @brief Get a uint64 property from DB (default column family)
  static uint64_t getIntProperty(rocksdb::DB* db, std::string const& property);
};

}  // namespace arangodb
