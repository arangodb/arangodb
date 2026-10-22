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

#include <rocksdb/compaction_filter.h>
#include <rocksdb/merge_operator.h>
#include <rocksdb/options.h>
#include <rocksdb/table_properties.h>

#include <cstddef>
#include <cstdint>

namespace arangodb::bitmap_index {

/// Merges bitmap deltas into bitmaps and count deltas into counts.
class MergeOperator final : public rocksdb::MergeOperator {
 public:
  char const* Name() const override { return "ArangoBitmapIndexMerge"; }

  bool FullMergeV2(MergeOperationInput const& in,
                   MergeOperationOutput* out) const override;

  bool PartialMergeMulti(rocksdb::Slice const& key,
                         std::deque<rocksdb::Slice> const& operands,
                         std::string* newValue,
                         rocksdb::Logger* logger) const override;
};

/// Removes empty bitmaps and zero counts. Merge operands are never filtered.
class CompactionFilter final : public rocksdb::CompactionFilter {
 public:
  char const* Name() const override { return "ArangoBitmapIndexFilter"; }

  Decision FilterV2(int level, rocksdb::Slice const& key, ValueType valueType,
                    rocksdb::Slice const& existingValue, std::string* newValue,
                    std::string* skipUntil) const override;
};

class CompactionFilterFactory final : public rocksdb::CompactionFilterFactory {
 public:
  char const* Name() const override { return "ArangoBitmapIndexFilter"; }

  std::unique_ptr<rocksdb::CompactionFilter> CreateCompactionFilter(
      rocksdb::CompactionFilter::Context const& context) override;
};

/// Requests compaction of SST files that hold many unmerged operands.
class MergeOperandCollector final : public rocksdb::TablePropertiesCollector {
 public:
  static constexpr std::uint64_t kMinOperands = 1000;
  static constexpr double kMaxOperandShare = 0.2;

  rocksdb::Status AddUserKey(rocksdb::Slice const& key,
                             rocksdb::Slice const& value,
                             rocksdb::EntryType type,
                             rocksdb::SequenceNumber seq,
                             std::uint64_t fileSize) override;
  rocksdb::Status Finish(rocksdb::UserCollectedProperties* properties) override;
  rocksdb::UserCollectedProperties GetReadableProperties() const override;
  char const* Name() const override { return "ArangoBitmapIndexOperands"; }
  bool NeedCompact() const override;

 private:
  std::uint64_t _entries = 0;
  std::uint64_t _operands = 0;
};

class MergeOperandCollectorFactory final
    : public rocksdb::TablePropertiesCollectorFactory {
 public:
  rocksdb::TablePropertiesCollector* CreateTablePropertiesCollector(
      rocksdb::TablePropertiesCollectorFactory::Context context) override;
  char const* Name() const override { return "ArangoBitmapIndexOperands"; }
};

/// at this many merge operands in the memtable RocksDB writes a full value
inline constexpr std::size_t kMaxSuccessiveMerges = 32;

/// set merge operator, compaction filter and properties collector for the
/// bitmap index column family
void configureColumnFamily(rocksdb::ColumnFamilyOptions& options);

}  // namespace arangodb::bitmap_index
