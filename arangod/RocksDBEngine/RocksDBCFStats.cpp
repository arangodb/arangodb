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

#include "RocksDBCFStats.h"

#include "Basics/StringUtils.h"

#include <absl/strings/str_split.h>
#include <absl/strings/match.h>

#include <charconv>

namespace arangodb {

namespace {

// Compaction stat property names as used in RocksDB's map output
// Format: "compaction.<Level>.<PropertyName>"
constexpr std::string_view kCompactionPrefix = "compaction.";

// Property names from RocksDB's LevelStat
constexpr std::string_view kNumFiles = "NumFiles";
constexpr std::string_view kCompactedFiles = "CompactedFiles";
constexpr std::string_view kSizeBytes = "SizeBytes";
constexpr std::string_view kScore = "Score";
constexpr std::string_view kReadGB = "ReadGB";
constexpr std::string_view kRnGB = "RnGB";
constexpr std::string_view kRnp1GB = "Rnp1GB";
constexpr std::string_view kWriteGB = "WriteGB";
constexpr std::string_view kWnewGB = "WnewGB";
constexpr std::string_view kMovedGB = "MovedGB";
constexpr std::string_view kWriteAmp = "WriteAmp";
constexpr std::string_view kReadMBps = "ReadMBps";
constexpr std::string_view kWriteMBps = "WriteMBps";
constexpr std::string_view kCompSec = "CompSec";
constexpr std::string_view kCompMergeCPU = "CompMergeCPU";
constexpr std::string_view kCompCount = "CompCount";
constexpr std::string_view kAvgSec = "AvgSec";
constexpr std::string_view kKeyIn = "KeyIn";
constexpr std::string_view kKeyDrop = "KeyDrop";
constexpr std::string_view kRblobGB = "RblobGB";
constexpr std::string_view kWblobGB = "WblobGB";

}  // namespace

uint64_t RocksDBCFStatsCollector::getIntProperty(
    rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cfHandle,
    std::string const& property) {
  uint64_t value = 0;
  db->GetIntProperty(cfHandle, property, &value);
  return value;
}

uint64_t RocksDBCFStatsCollector::getIntProperty(rocksdb::DB* db,
                                                 std::string const& property) {
  uint64_t value = 0;
  db->GetIntProperty(property, &value);
  return value;
}

void RocksDBCFStatsCollector::parseCompactionStats(
    std::map<std::string, std::string> const& cfStatsMap,
    ColumnFamilyStats& stats) {
  // Group stats by level
  // Keys are like: "compaction.L0.ReadGB", "compaction.Sum.WriteGB", etc.
  std::map<std::string, CompactionLevelStats> levelMap;

  for (auto const& [key, value] : cfStatsMap) {
    if (!absl::StartsWith(key, kCompactionPrefix)) {
      continue;
    }

    // Parse "compaction.<Level>.<Property>"
    auto rest = key.substr(kCompactionPrefix.size());
    auto dotPos = rest.find('.');
    if (dotPos == std::string::npos) {
      continue;
    }

    std::string level = std::string(rest.substr(0, dotPos));
    std::string property = std::string(rest.substr(dotPos + 1));

    // Get or create the level stats entry
    auto& levelStats = levelMap[level];
    levelStats.level = level;

    // Parse each property
    if (property == kNumFiles) {
      levelStats.numFiles = basics::StringUtils::uint64(value);
    } else if (property == kCompactedFiles) {
      levelStats.compactedFiles = basics::StringUtils::uint64(value);
    } else if (property == kSizeBytes) {
      levelStats.sizeBytes = basics::StringUtils::doubleDecimal(value);
    } else if (property == kScore) {
      levelStats.score = basics::StringUtils::doubleDecimal(value);
    } else if (property == kReadGB) {
      levelStats.readGB = basics::StringUtils::doubleDecimal(value);
    } else if (property == kRnGB) {
      levelStats.rnGB = basics::StringUtils::doubleDecimal(value);
    } else if (property == kRnp1GB) {
      levelStats.rnp1GB = basics::StringUtils::doubleDecimal(value);
    } else if (property == kWriteGB) {
      levelStats.writeGB = basics::StringUtils::doubleDecimal(value);
    } else if (property == kWnewGB) {
      levelStats.wnewGB = basics::StringUtils::doubleDecimal(value);
    } else if (property == kMovedGB) {
      levelStats.movedGB = basics::StringUtils::doubleDecimal(value);
    } else if (property == kWriteAmp) {
      levelStats.writeAmp = basics::StringUtils::doubleDecimal(value);
    } else if (property == kReadMBps) {
      levelStats.readMBps = basics::StringUtils::doubleDecimal(value);
    } else if (property == kWriteMBps) {
      levelStats.writeMBps = basics::StringUtils::doubleDecimal(value);
    } else if (property == kCompSec) {
      levelStats.compSec = basics::StringUtils::doubleDecimal(value);
    } else if (property == kCompMergeCPU) {
      levelStats.compMergeCPUSec = basics::StringUtils::doubleDecimal(value);
    } else if (property == kCompCount) {
      levelStats.compCount = basics::StringUtils::uint64(value);
    } else if (property == kAvgSec) {
      levelStats.avgSec = basics::StringUtils::doubleDecimal(value);
    } else if (property == kKeyIn) {
      levelStats.keyIn = basics::StringUtils::uint64(value);
    } else if (property == kKeyDrop) {
      levelStats.keyDrop = basics::StringUtils::uint64(value);
    } else if (property == kRblobGB) {
      levelStats.rblobGB = basics::StringUtils::doubleDecimal(value);
    } else if (property == kWblobGB) {
      levelStats.wblobGB = basics::StringUtils::doubleDecimal(value);
    }
  }

  // Separate Sum from level stats
  for (auto& [level, levelStats] : levelMap) {
    if (level == "Sum") {
      stats.sumStats = std::move(levelStats);
    } else {
      stats.levelStats.push_back(std::move(levelStats));
    }
  }

  // Sort level stats by level name (L0, L1, L2, ...)
  std::sort(stats.levelStats.begin(), stats.levelStats.end(),
            [](CompactionLevelStats const& a, CompactionLevelStats const& b) {
              // Extract level number for proper sorting
              auto extractNum = [](std::string const& s) -> int {
                if (s.size() > 1 && s[0] == 'L') {
                  int num = 0;
                  std::from_chars(s.data() + 1, s.data() + s.size(), num);
                  return num;
                }
                return -1;
              };
              return extractNum(a.level) < extractNum(b.level);
            });
}

void RocksDBCFStatsCollector::parseWriteStallStats(
    std::map<std::string, std::string> const& writeStallMap,
    WriteStallStats& stats) {
  // Use the structured keys from WriteStallStatsMapKeys
  auto getValue = [&writeStallMap](std::string const& key) -> uint64_t {
    auto it = writeStallMap.find(key);
    if (it != writeStallMap.end()) {
      return basics::StringUtils::uint64(it->second);
    }
    return 0;
  };

  stats.totalDelays = getValue(rocksdb::WriteStallStatsMapKeys::TotalDelays());
  stats.totalStops = getValue(rocksdb::WriteStallStatsMapKeys::TotalStops());
  stats.cfL0FileCountLimitDelaysWithOngoingCompaction =
      getValue(rocksdb::WriteStallStatsMapKeys::
                   CFL0FileCountLimitDelaysWithOngoingCompaction());
  stats.cfL0FileCountLimitStopsWithOngoingCompaction =
      getValue(rocksdb::WriteStallStatsMapKeys::
                   CFL0FileCountLimitStopsWithOngoingCompaction());

  // Parse cause-condition counts
  stats.l0FileCountLimitDelays =
      getValue(rocksdb::WriteStallStatsMapKeys::CauseConditionCount(
          rocksdb::WriteStallCause::kL0FileCountLimit,
          rocksdb::WriteStallCondition::kDelayed));
  stats.l0FileCountLimitStops =
      getValue(rocksdb::WriteStallStatsMapKeys::CauseConditionCount(
          rocksdb::WriteStallCause::kL0FileCountLimit,
          rocksdb::WriteStallCondition::kStopped));
  stats.memtableLimitDelays =
      getValue(rocksdb::WriteStallStatsMapKeys::CauseConditionCount(
          rocksdb::WriteStallCause::kMemtableLimit,
          rocksdb::WriteStallCondition::kDelayed));
  stats.memtableLimitStops =
      getValue(rocksdb::WriteStallStatsMapKeys::CauseConditionCount(
          rocksdb::WriteStallCause::kMemtableLimit,
          rocksdb::WriteStallCondition::kStopped));
  stats.pendingCompactionBytesDelays =
      getValue(rocksdb::WriteStallStatsMapKeys::CauseConditionCount(
          rocksdb::WriteStallCause::kPendingCompactionBytes,
          rocksdb::WriteStallCondition::kDelayed));
  stats.pendingCompactionBytesStops =
      getValue(rocksdb::WriteStallStatsMapKeys::CauseConditionCount(
          rocksdb::WriteStallCause::kPendingCompactionBytes,
          rocksdb::WriteStallCondition::kStopped));
}

void RocksDBCFStatsCollector::parseBlockCacheEntryStats(
    std::map<std::string, std::string> const& cacheStatsMap,
    BlockCacheStats& stats) {
  // Parse using BlockCacheEntryStatsMapKeys
  auto const getStringValue =
      [&cacheStatsMap](std::string const& key) -> std::string {
    auto it = cacheStatsMap.find(key);
    if (it != cacheStatsMap.end()) {
      return it->second;
    }
    return "";
  };

  auto getUint64Value = [&cacheStatsMap](std::string const& key) -> uint64_t {
    auto it = cacheStatsMap.find(key);
    if (it != cacheStatsMap.end()) {
      return basics::StringUtils::uint64(it->second);
    }
    return 0;
  };

  auto getDoubleValue = [&cacheStatsMap](std::string const& key) -> double {
    auto it = cacheStatsMap.find(key);
    if (it != cacheStatsMap.end()) {
      return basics::StringUtils::doubleDecimal(it->second);
    }
    return 0.0;
  };

  stats.cacheId =
      getStringValue(rocksdb::BlockCacheEntryStatsMapKeys::CacheId());
  stats.capacityBytes = getUint64Value(
      rocksdb::BlockCacheEntryStatsMapKeys::CacheCapacityBytes());
  stats.lastCollectionDurationSecs = getDoubleValue(
      rocksdb::BlockCacheEntryStatsMapKeys::LastCollectionDurationSeconds());
  stats.lastCollectionAgeSecs = static_cast<uint64_t>(getDoubleValue(
      rocksdb::BlockCacheEntryStatsMapKeys::LastCollectionAgeSeconds()));

  // Parse per-role stats
  // CacheEntryRole is defined in rocksdb/cache.h
  for (size_t i = 0;
       i < static_cast<size_t>(rocksdb::CacheEntryRole::kMisc) + 1; ++i) {
    auto role = static_cast<rocksdb::CacheEntryRole>(i);

    BlockCacheRoleStats roleStats;
    roleStats.count =
        getUint64Value(rocksdb::BlockCacheEntryStatsMapKeys::EntryCount(role));
    roleStats.usedBytes =
        getUint64Value(rocksdb::BlockCacheEntryStatsMapKeys::UsedBytes(role));
    roleStats.usedPercent =
        getDoubleValue(rocksdb::BlockCacheEntryStatsMapKeys::UsedPercent(role));

    if (roleStats.count > 0 || roleStats.usedBytes > 0) {
      // Get role name - we'll use a simple mapping
      std::string roleName;
      switch (role) {
        case rocksdb::CacheEntryRole::kDataBlock:
          roleName = "DataBlock";
          break;
        case rocksdb::CacheEntryRole::kFilterBlock:
          roleName = "FilterBlock";
          break;
        case rocksdb::CacheEntryRole::kFilterMetaBlock:
          roleName = "FilterMetaBlock";
          break;
        case rocksdb::CacheEntryRole::kDeprecatedFilterBlock:
          roleName = "DeprecatedFilterBlock";
          break;
        case rocksdb::CacheEntryRole::kIndexBlock:
          roleName = "IndexBlock";
          break;
        case rocksdb::CacheEntryRole::kOtherBlock:
          roleName = "OtherBlock";
          break;
        case rocksdb::CacheEntryRole::kWriteBuffer:
          roleName = "WriteBuffer";
          break;
        case rocksdb::CacheEntryRole::kCompressionDictionaryBuildingBuffer:
          roleName = "CompressionDictBuildBuffer";
          break;
        case rocksdb::CacheEntryRole::kFilterConstruction:
          roleName = "FilterConstruction";
          break;
        case rocksdb::CacheEntryRole::kBlockBasedTableReader:
          roleName = "BlockBasedTableReader";
          break;
        case rocksdb::CacheEntryRole::kFileMetadata:
          roleName = "FileMetadata";
          break;
        case rocksdb::CacheEntryRole::kBlobValue:
          roleName = "BlobValue";
          break;
        case rocksdb::CacheEntryRole::kBlobCache:
          roleName = "BlobCache";
          break;
        case rocksdb::CacheEntryRole::kMisc:
          roleName = "Misc";
          break;
        default:
          roleName = "Unknown";
          break;
      }
      stats.roleStats[roleName] = roleStats;
    }
  }
}

ColumnFamilyStats RocksDBCFStatsCollector::collect(
    rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cfHandle,
    std::string const& cfName) {
  ColumnFamilyStats stats;
  stats.columnFamilyName = cfName;

  // 1. Get CF stats as a map (includes compaction stats and write stall stats)
  std::map<std::string, std::string> cfStatsMap;
  if (db->GetMapProperty(cfHandle, rocksdb::DB::Properties::kCFStats,
                         &cfStatsMap)) {
    parseCompactionStats(cfStatsMap, stats);
  }

  // 2. Get write stall stats separately for more structured access
  std::map<std::string, std::string> writeStallMap;
  if (db->GetMapProperty(cfHandle, rocksdb::DB::Properties::kCFWriteStallStats,
                         &writeStallMap)) {
    parseWriteStallStats(writeStallMap, stats.writeStallStats);
  }

  // 3. Get integer properties
  stats.numImmutableMemTable = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kNumImmutableMemTable);
  stats.numImmutableMemTableFlushed = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kNumImmutableMemTableFlushed);
  stats.memTableFlushPending = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kMemTableFlushPending);
  stats.compactionPending =
      getIntProperty(db, cfHandle, rocksdb::DB::Properties::kCompactionPending);
  stats.curSizeActiveMemTable = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kCurSizeActiveMemTable);
  stats.curSizeAllMemTables = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kCurSizeAllMemTables);
  stats.sizeAllMemTables =
      getIntProperty(db, cfHandle, rocksdb::DB::Properties::kSizeAllMemTables);
  stats.numEntriesActiveMemTable = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kNumEntriesActiveMemTable);
  stats.numEntriesImmMemTables = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kNumEntriesImmMemTables);
  stats.numDeletesActiveMemTable = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kNumDeletesActiveMemTable);
  stats.numDeletesImmMemTables = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kNumDeletesImmMemTables);
  stats.estimateNumKeys =
      getIntProperty(db, cfHandle, rocksdb::DB::Properties::kEstimateNumKeys);
  stats.estimateTableReadersMem = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kEstimateTableReadersMem);
  stats.numLiveVersions =
      getIntProperty(db, cfHandle, rocksdb::DB::Properties::kNumLiveVersions);
  stats.estimateLiveDataSize = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kEstimateLiveDataSize);
  stats.liveSstFilesSize =
      getIntProperty(db, cfHandle, rocksdb::DB::Properties::kLiveSstFilesSize);
  stats.estimatePendingCompactionBytes = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kEstimatePendingCompactionBytes);

  // 4. Blob file stats
  stats.numBlobFiles =
      getIntProperty(db, cfHandle, rocksdb::DB::Properties::kNumBlobFiles);
  stats.liveBlobFileSize =
      getIntProperty(db, cfHandle, rocksdb::DB::Properties::kLiveBlobFileSize);
  stats.liveBlobFileGarbageSize = getIntProperty(
      db, cfHandle, rocksdb::DB::Properties::kLiveBlobFileGarbageSize);

  return stats;
}

DatabaseStats RocksDBCFStatsCollector::collectDatabaseStats(rocksdb::DB* db) {
  DatabaseStats stats;

  // DB-wide integer properties
  stats.numSnapshots =
      getIntProperty(db, rocksdb::DB::Properties::kNumSnapshots);
  stats.oldestSnapshotTime =
      getIntProperty(db, rocksdb::DB::Properties::kOldestSnapshotTime);
  stats.numRunningCompactions =
      getIntProperty(db, rocksdb::DB::Properties::kNumRunningCompactions);
  stats.numRunningFlushes =
      getIntProperty(db, rocksdb::DB::Properties::kNumRunningFlushes);
  stats.blockCacheCapacity =
      getIntProperty(db, rocksdb::DB::Properties::kBlockCacheCapacity);
  stats.blockCacheUsage =
      getIntProperty(db, rocksdb::DB::Properties::kBlockCacheUsage);
  stats.blockCachePinnedUsage =
      getIntProperty(db, rocksdb::DB::Properties::kBlockCachePinnedUsage);

  // Block cache entry stats
  stats.blockCacheStats = collectBlockCacheStats(db);

  return stats;
}

std::optional<BlockCacheStats> RocksDBCFStatsCollector::collectBlockCacheStats(
    rocksdb::DB* db) {
  std::map<std::string, std::string> cacheStatsMap;
  if (!db->GetMapProperty(rocksdb::DB::Properties::kBlockCacheEntryStats,
                          &cacheStatsMap)) {
    return std::nullopt;
  }

  BlockCacheStats stats;
  parseBlockCacheEntryStats(cacheStatsMap, stats);
  return stats;
}

}  // namespace arangodb
