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

#include "RocksDBBitmapIndexMergeOperator.h"

#include "Basics/debugging.h"
#include "RocksDBEngine/RocksDBBitmapIndexFormat.h"

#include <string>

namespace arangodb::bitmap_index {

namespace {
bool isCount(ValueTag tag) {
  return tag == ValueTag::Count || tag == ValueTag::CountDelta;
}

std::string_view view(rocksdb::Slice s) { return {s.data(), s.size()}; }
}  // namespace

bool MergeOperator::FullMergeV2(MergeOperationInput const& in,
                                MergeOperationOutput* out) const {
  TRI_ASSERT(!in.operand_list.empty());
  auto firstTag = tagOf(view(in.operand_list.front()));
  if (!firstTag.has_value()) {
    return false;
  }

  try {
    if (isCount(*firstTag)) {
      std::int64_t total = 0;
      if (in.existing_value != nullptr) {
        total = decodeCount(view(*in.existing_value));
      }
      for (auto const& op : in.operand_list) {
        total += decodeCount(view(op));
      }
      out->new_value = encodeCount(ValueTag::Count, total);
      return true;
    }

    roaring::Roaring bitmap;
    if (in.existing_value != nullptr) {
      bitmap = decodeBitmap(view(*in.existing_value));
    }
    roaring::Roaring adds;
    roaring::Roaring removes;
    for (auto const& op : in.operand_list) {
      auto tag = tagOf(view(op));
      if (tag == ValueTag::Bitmap) {
        bitmap = decodeBitmap(view(op));
        continue;
      }
      if (tag != ValueTag::Delta) {
        return false;
      }
      decodeDelta(view(op), adds, removes);
      bitmap |= adds;
      bitmap -= removes;
    }
    out->new_value = encodeBitmap(bitmap);
    return true;
  } catch (...) {
    return false;
  }
}

bool MergeOperator::PartialMergeMulti(
    rocksdb::Slice const& /*key*/, std::deque<rocksdb::Slice> const& operands,
    std::string* newValue, rocksdb::Logger* /*logger*/) const {
  TRI_ASSERT(operands.size() >= 2);
  auto firstTag = tagOf(view(operands.front()));
  if (!firstTag.has_value() || *firstTag == ValueTag::Bitmap ||
      *firstTag == ValueTag::Count) {
    return false;
  }

  try {
    if (*firstTag == ValueTag::CountDelta) {
      std::int64_t total = 0;
      for (auto const& op : operands) {
        if (tagOf(view(op)) != ValueTag::CountDelta) {
          return false;
        }
        total += decodeCount(view(op));
      }
      *newValue = encodeCount(ValueTag::CountDelta, total);
      return true;
    }

    // a document id is added at most once and removed at most once, in that
    // order. an add followed by a remove inside the run cancels out; a remove
    // whose add is older than the run is kept.
    roaring::Roaring adds;
    roaring::Roaring removes;
    roaring::Roaring opAdds;
    roaring::Roaring opRemoves;
    for (auto const& op : operands) {
      if (tagOf(view(op)) != ValueTag::Delta) {
        return false;
      }
      decodeDelta(view(op), opAdds, opRemoves);
      roaring::Roaring cancelled = adds & opRemoves;
      adds -= opRemoves;
      adds |= opAdds;
      opRemoves -= cancelled;
      removes |= opRemoves;
      removes -= opAdds;
    }
    adds.runOptimize();
    removes.runOptimize();
    *newValue = encodeDelta(adds, removes);
    return true;
  } catch (...) {
    return false;
  }
}

rocksdb::CompactionFilter::Decision CompactionFilter::FilterV2(
    int /*level*/, rocksdb::Slice const& /*key*/, ValueType valueType,
    rocksdb::Slice const& existingValue, std::string* /*newValue*/,
    std::string* /*skipUntil*/) const {
  if (valueType != ValueType::kValue) {
    // merge operands must stay: dropping a remove would resurrect an id, and
    // TransactionDB relies on them for conflict checks
    return Decision::kKeep;
  }
  try {
    auto tag = tagOf(view(existingValue));
    if (tag == ValueTag::Count) {
      return decodeCount(view(existingValue)) == 0 ? Decision::kRemove
                                                   : Decision::kKeep;
    }
    if (tag == ValueTag::Bitmap) {
      // an empty bitmap and a missing key mean the same to every reader
      return decodeBitmap(view(existingValue)).isEmpty() ? Decision::kRemove
                                                         : Decision::kKeep;
    }
  } catch (...) {
  }
  return Decision::kKeep;
}

std::unique_ptr<rocksdb::CompactionFilter>
CompactionFilterFactory::CreateCompactionFilter(
    rocksdb::CompactionFilter::Context const& /*context*/) {
  return std::make_unique<CompactionFilter>();
}

rocksdb::Status MergeOperandCollector::AddUserKey(
    rocksdb::Slice const& /*key*/, rocksdb::Slice const& /*value*/,
    rocksdb::EntryType type, rocksdb::SequenceNumber /*seq*/,
    std::uint64_t /*fileSize*/) {
  ++_entries;
  if (type == rocksdb::kEntryMerge) {
    ++_operands;
  }
  return rocksdb::Status::OK();
}

rocksdb::Status MergeOperandCollector::Finish(
    rocksdb::UserCollectedProperties* properties) {
  *properties = GetReadableProperties();
  return rocksdb::Status::OK();
}

rocksdb::UserCollectedProperties MergeOperandCollector::GetReadableProperties()
    const {
  return {{"arango.bitmap.entries", std::to_string(_entries)},
          {"arango.bitmap.mergeOperands", std::to_string(_operands)}};
}

bool MergeOperandCollector::NeedCompact() const {
  return _operands >= kMinOperands &&
         static_cast<double>(_operands) >
             kMaxOperandShare * static_cast<double>(_entries);
}

rocksdb::TablePropertiesCollector*
MergeOperandCollectorFactory::CreateTablePropertiesCollector(
    rocksdb::TablePropertiesCollectorFactory::Context /*context*/) {
  return new MergeOperandCollector();
}

void configureColumnFamily(rocksdb::ColumnFamilyOptions& options) {
  options.merge_operator = std::make_shared<MergeOperator>();
  options.compaction_filter_factory =
      std::make_shared<CompactionFilterFactory>();
  options.table_properties_collector_factories.emplace_back(
      std::make_shared<MergeOperandCollectorFactory>());
  options.max_successive_merges = kMaxSuccessiveMerges;
}

}  // namespace arangodb::bitmap_index
