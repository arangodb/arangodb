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

#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBBitmapIndex.h"
#include "RocksDBEngine/RocksDBBitmapIndexFormat.h"
#include "RocksDBEngine/RocksDBBitmapIndexMergeOperator.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBFormat.h"

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

/// objectId | [values...] (| shard), as the bitmap index builds its keys
std::string key(std::string_view valuesJson,
                std::optional<std::uint64_t> shard = std::nullopt) {
  auto values = velocypack::Parser::fromJson(valuesJson.data(),
                                             valuesJson.size());
  std::string out;
  rocksutils::uint64ToPersistent(out, 42);
  out.append(reinterpret_cast<char const*>(values->slice().begin()),
             values->slice().byteSize());
  if (shard.has_value()) {
    appendShard(out, *shard);
  }
  return out;
}

int compare(std::string const& lhs, std::string const& rhs) {
  static RocksDBVPackComparator<
      basics::VelocyPackHelper::SortingMethod::Correct> const cmp;
  return cmp.Compare(lhs, rhs);
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

TEST(BitmapIndexFormatTest, values_aql_considers_equal_share_a_key) {
  EXPECT_EQ(0, compare(key(R"(["v/1", 1])"), key(R"(["v/1", 1.0])")));
  EXPECT_EQ(0, compare(key(R"(["v/1", 0])"), key(R"(["v/1", -0.0])")));
  EXPECT_EQ(0, compare(key(R"(["v/1", {"a": 1, "b": [2]}])"),
                       key(R"(["v/1", {"b": [2.0], "a": 1}])")));
  EXPECT_EQ(0, compare(key(R"(["v/1", [1, "x"]])", 7),
                       key(R"(["v/1", [1.0, "x"]])", 7)));
}

TEST(BitmapIndexFormatTest, distinct_values_have_distinct_keys) {
  EXPECT_NE(0, compare(key(R"(["v/1", 1])"), key(R"(["v/1", "1"])")));
  EXPECT_NE(0, compare(key(R"(["v/1", "a"])"), key(R"(["v/1", "ab"])")));
  EXPECT_NE(0, compare(key(R"(["v/1", [1]])"), key(R"(["v/1", 1])")));
  EXPECT_NE(0, compare(key(R"(["v/1", null])"), key(R"(["v/1", false])")));
  EXPECT_NE(0, compare(key(R"(["v/1", "a"])"), key(R"(["v/2", "a"])")));
}

TEST(BitmapIndexFormatTest, count_key_sorts_before_shard_keys) {
  std::string countKey = key(R"(["v/1", "a"])");
  std::string first = key(R"(["v/1", "a"])", 0);
  std::string second = key(R"(["v/1", "a"])", 1);
  std::string large = key(R"(["v/1", "a"])", 1ULL << 40);
  std::string nextElement = key(R"(["v/1", "b"])");
  EXPECT_LT(compare(countKey, first), 0);
  EXPECT_LT(compare(first, second), 0);
  EXPECT_LT(compare(second, large), 0);
  EXPECT_LT(compare(large, nextElement), 0);
  // a seek with an equal value of different bytes lands on the same keys
  EXPECT_EQ(0, compare(key(R"(["v/1", "a"])", 1), second));
  EXPECT_LT(compare(key(R"(["v/1", "a"])"), key(R"(["v/1", "a"])", 0)), 0);
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

TEST(BitmapIndexComparatorDBTest, merges_of_equal_values_meet_in_one_key) {
  static RocksDBVPackComparator<
      basics::VelocyPackHelper::SortingMethod::Correct> const cmp;
  auto path = std::filesystem::temp_directory_path() /
              ("arangodb-bitmap-index-cmp-test-" + std::to_string(::getpid()));
  std::filesystem::remove_all(path);
  rocksdb::Options options;
  options.create_if_missing = true;
  options.comparator = &cmp;
  configureColumnFamily(options);
  rocksdb::DB* raw = nullptr;
  ASSERT_TRUE(rocksdb::DB::Open(options, path.string(), &raw).ok());
  std::unique_ptr<rocksdb::DB> db(raw);

  auto merge = [&](std::string const& k, std::string const& v) {
    ASSERT_TRUE(db->Merge(rocksdb::WriteOptions(), k, v).ok());
  };
  merge(key(R"(["v/1", 1])", 3), addDelta({1}));
  merge(key(R"(["v/1", 1.0])", 3), addDelta({2}));
  ASSERT_TRUE(db->Flush(rocksdb::FlushOptions()).ok());
  merge(key(R"(["v/1", {"a": 1, "b": 2}])", 3), addDelta({5}));
  merge(key(R"(["v/1", {"b": 2, "a": 1.0}])", 3), addDelta({6}));
  merge(key(R"(["v/1", 1])", 3), removeDelta({1}));

  std::string value;
  ASSERT_TRUE(
      db->Get(rocksdb::ReadOptions(), key(R"(["v/1", 1.0])", 3), &value).ok());
  EXPECT_EQ((std::vector<std::uint32_t>{2}), contents(value));
  ASSERT_TRUE(db->Get(rocksdb::ReadOptions(),
                      key(R"(["v/1", {"a": 1, "b": 2}])", 3), &value)
                  .ok());
  EXPECT_EQ((std::vector<std::uint32_t>{5, 6}), contents(value));

  // one key per distinct value
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(rocksdb::ReadOptions()));
  std::size_t keys = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    ++keys;
  }
  EXPECT_EQ(2U, keys);

  it.reset();
  db.reset();
  std::filesystem::remove_all(path);
}

namespace {
std::string stored(std::string_view json) {
  auto value = velocypack::Parser::fromJson(json.data(), json.size());
  velocypack::Builder out;
  EXPECT_TRUE(ScalarValues::add(out, value->slice())) << json;
  return {reinterpret_cast<char const*>(out.data()), out.size()};
}
}  // namespace

TEST(BitmapIndexScalarValuesTest, equal_numbers_have_equal_bytes) {
  EXPECT_EQ(stored("1"), stored("1.0"));
  EXPECT_EQ(stored("0"), stored("-0.0"));
  EXPECT_NE(stored("1"), stored("\"1\""));
  EXPECT_NE(stored("\"a\""), stored("\"ab\""));
  EXPECT_NE(stored("true"), stored("null"));
}

TEST(BitmapIndexScalarValuesTest, arrays_and_objects_are_refused) {
  for (std::string_view json : {"[1]", "{\"a\":1}", "[]"}) {
    auto value = velocypack::Parser::fromJson(json.data(), json.size());
    velocypack::Builder out;
    EXPECT_FALSE(ScalarValues::add(out, value->slice())) << json;
    EXPECT_TRUE(out.isEmpty()) << json;
  }
}
