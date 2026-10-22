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

#include "gtest/gtest.h"

#include "RocksDBEngine/RocksDBBitmapIndexFormat.h"
#include "RocksDBEngine/RocksDBBitmapIndexMergeOperator.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

using namespace arangodb;
using namespace arangodb::bitmap_index;

namespace {

std::string normalized(std::string_view json) {
  auto builder = velocypack::Parser::fromJson(json.data(), json.size());
  std::string out;
  EXPECT_TRUE(appendNormalized(out, builder->slice()));
  return out;
}

std::string addDelta(std::vector<std::uint32_t> const& offsets) {
  roaring::Roaring adds;
  for (auto o : offsets) {
    adds.add(o);
  }
  return encodeDelta(adds, roaring::Roaring{});
}

std::string removeDelta(std::vector<std::uint32_t> const& offsets) {
  roaring::Roaring removes;
  for (auto o : offsets) {
    removes.add(o);
  }
  return encodeDelta(roaring::Roaring{}, removes);
}

std::vector<std::uint32_t> contents(std::string_view value) {
  roaring::Roaring bitmap = decodeBitmap(value);
  std::vector<std::uint32_t> out(bitmap.cardinality());
  bitmap.toUint32Array(out.data());
  return out;
}

class BitmapIndexDBTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<int> counter{0};
    _path = std::filesystem::temp_directory_path() /
            ("arangodb-bitmap-index-test-" + std::to_string(::getpid()) + "-" +
             std::to_string(counter++));
    std::filesystem::remove_all(_path);

    rocksdb::Options options;
    options.create_if_missing = true;
    configureColumnFamily(options);
    rocksdb::DB* db = nullptr;
    auto s = rocksdb::DB::Open(options, _path.string(), &db);
    ASSERT_TRUE(s.ok()) << s.ToString();
    _db.reset(db);
  }

  void TearDown() override {
    _db.reset();
    std::filesystem::remove_all(_path);
  }

  void merge(std::string const& key, std::string const& value) {
    auto s = _db->Merge(rocksdb::WriteOptions(), key, value);
    ASSERT_TRUE(s.ok()) << s.ToString();
  }

  std::optional<std::string> get(std::string const& key) {
    std::string value;
    auto s = _db->Get(rocksdb::ReadOptions(), key, &value);
    if (s.IsNotFound()) {
      return std::nullopt;
    }
    EXPECT_TRUE(s.ok()) << s.ToString();
    return value;
  }

  void flush() {
    auto s = _db->Flush(rocksdb::FlushOptions());
    ASSERT_TRUE(s.ok()) << s.ToString();
  }

  void compact() {
    rocksdb::CompactRangeOptions options;
    options.bottommost_level_compaction =
        rocksdb::BottommostLevelCompaction::kForce;
    auto s = _db->CompactRange(options, nullptr, nullptr);
    ASSERT_TRUE(s.ok()) << s.ToString();
  }

  std::filesystem::path _path;
  std::unique_ptr<rocksdb::DB> _db;
};

}  // namespace

TEST(BitmapIndexFormatTest, shard_encoding_round_trips_and_sorts) {
  std::vector<std::uint64_t> shards = {0, 1, 255, 256, 65535, 1ULL << 40,
                                       (1ULL << (64 - kShardBits)) - 1};
  std::string previous;
  for (auto shard : shards) {
    std::string encoded;
    appendShard(encoded, shard);
    ASSERT_EQ(kShardBytes, encoded.size());
    EXPECT_EQ(shard, readShard(encoded.data()));
    EXPECT_LT(previous, encoded);
    previous = encoded;
  }
}

TEST(BitmapIndexFormatTest, ids_split_into_shard_and_offset) {
  std::uint64_t id = (12345ULL << kShardBits) | 678;
  EXPECT_EQ(12345ULL, shardOf(id));
  EXPECT_EQ(678U, offsetOf(id));
  EXPECT_EQ(id, makeId(shardOf(id), offsetOf(id)));
}

TEST(BitmapIndexFormatTest, equal_numbers_share_one_term) {
  EXPECT_EQ(normalized("1"), normalized("1.0"));
  EXPECT_EQ(normalized("0"), normalized("-0.0"));
  EXPECT_NE(normalized("1"), normalized("\"1\""));
  EXPECT_NE(normalized("\"a\""), normalized("\"ab\""));
  EXPECT_NE(normalized("true"), normalized("null"));
}

TEST(BitmapIndexFormatTest, non_scalars_are_not_terms) {
  for (std::string_view json : {"[1]", "{\"a\":1}"}) {
    auto builder = velocypack::Parser::fromJson(json.data(), json.size());
    std::string out;
    EXPECT_FALSE(appendNormalized(out, builder->slice()));
    EXPECT_TRUE(out.empty());
  }
}

TEST(BitmapIndexFormatTest, counts_round_trip) {
  EXPECT_EQ(-3, decodeCount(encodeCount(ValueTag::CountDelta, -3)));
  EXPECT_EQ(42, decodeCount(encodeCount(ValueTag::Count, 42)));
}

TEST(BitmapIndexFormatTest, collector_requests_compaction_for_many_operands) {
  MergeOperandCollector few;
  for (int i = 0; i < 500; ++i) {
    ASSERT_TRUE(few.AddUserKey("k", "v", rocksdb::kEntryMerge, 0, 0).ok());
  }
  EXPECT_FALSE(few.NeedCompact());

  MergeOperandCollector many;
  for (int i = 0; i < 2000; ++i) {
    ASSERT_TRUE(many.AddUserKey("k", "v", rocksdb::kEntryMerge, 0, 0).ok());
    ASSERT_TRUE(many.AddUserKey("k", "v", rocksdb::kEntryPut, 0, 0).ok());
  }
  EXPECT_TRUE(many.NeedCompact());

  MergeOperandCollector mostlyValues;
  for (int i = 0; i < 20000; ++i) {
    ASSERT_TRUE(
        mostlyValues.AddUserKey("k", "v", rocksdb::kEntryPut, 0, 0).ok());
  }
  for (int i = 0; i < 1500; ++i) {
    ASSERT_TRUE(
        mostlyValues.AddUserKey("k", "v", rocksdb::kEntryMerge, 0, 0).ok());
  }
  EXPECT_FALSE(mostlyValues.NeedCompact());
}

TEST_F(BitmapIndexDBTest, deltas_merge_into_a_bitmap) {
  merge("b", addDelta({1, 5}));
  merge("b", addDelta({7}));
  merge("b", removeDelta({5}));
  auto value = get("b");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ((std::vector<std::uint32_t>{1, 7}), contents(*value));
}

TEST_F(BitmapIndexDBTest, deltas_across_flushes_merge_onto_the_base) {
  merge("b", addDelta({1, 2, 3}));
  flush();
  compact();
  merge("b", removeDelta({2}));
  merge("b", addDelta({10}));
  flush();
  merge("b", addDelta({11}));
  merge("b", removeDelta({10}));
  auto value = get("b");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ((std::vector<std::uint32_t>{1, 3, 11}), contents(*value));

  compact();
  value = get("b");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(ValueTag::Bitmap, tagOf(*value));
  EXPECT_EQ((std::vector<std::uint32_t>{1, 3, 11}), contents(*value));
}

TEST_F(BitmapIndexDBTest, partial_merge_keeps_removes_of_older_adds) {
  merge("b", addDelta({4}));
  flush();
  compact();
  // these two runs only meet each other in compaction, without the base
  merge("b", addDelta({8}));
  merge("b", removeDelta({4}));
  flush();
  merge("b", removeDelta({8}));
  flush();
  auto value = get("b");
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(contents(*value).empty());
}

TEST_F(BitmapIndexDBTest, counts_are_summed) {
  merge("c", encodeCount(ValueTag::CountDelta, 1));
  merge("c", encodeCount(ValueTag::CountDelta, 1));
  flush();
  merge("c", encodeCount(ValueTag::CountDelta, -1));
  merge("c", encodeCount(ValueTag::CountDelta, 3));
  auto value = get("c");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(4, decodeCount(*value));
}

TEST_F(BitmapIndexDBTest, compaction_removes_empty_bitmaps_and_zero_counts) {
  merge("b", addDelta({3}));
  merge("c", encodeCount(ValueTag::CountDelta, 1));
  merge("keep", addDelta({9}));
  flush();
  compact();
  merge("b", removeDelta({3}));
  merge("c", encodeCount(ValueTag::CountDelta, -1));
  flush();
  // the first compaction merges the operands into empty full values, the
  // second one sees them as plain values and drops them
  compact();
  compact();
  EXPECT_FALSE(get("b").has_value());
  EXPECT_FALSE(get("c").has_value());
  auto keep = get("keep");
  ASSERT_TRUE(keep.has_value());
  EXPECT_EQ((std::vector<std::uint32_t>{9}), contents(*keep));
}

TEST_F(BitmapIndexDBTest, many_memtable_operands_collapse_into_a_full_value) {
  for (std::uint32_t i = 0; i < 3 * kMaxSuccessiveMerges; ++i) {
    merge("b", addDelta({i}));
  }
  auto value = get("b");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(3 * kMaxSuccessiveMerges, contents(*value).size());
}
