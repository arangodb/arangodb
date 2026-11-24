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

#include "Basics/AttributeNameParser.h"
#include "RocksDBEngine/InvertedIndex/RocksDBInvertedIndex.h"
#include "RocksDBEngine/InvertedIndex/Expr.h"
#include "RocksDBEngine/InvertedIndex/Format.h"
#include "RocksDBEngine/InvertedIndex/MergeOperator.h"
#include "RocksDBEngine/InvertedIndex/PendingWrites.h"
#include "RocksDBEngine/RocksDBMethodsMemoryTracker.h"
#include "RocksDBEngine/Methods/RocksDBBatchedMethods.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

using namespace arangodb;
using namespace arangodb::inverted_index;

namespace {

std::optional<std::string> term(std::string_view json) {
  auto value = velocypack::Parser::fromJson(json.data(), json.size());
  return normalizedTerm(value->slice());
}

/// key of the count of a term given as JSON, as the RocksDB inverted index
/// builds it
std::string key(std::string_view json) { return termKey(42, *term(json)); }

std::string key(std::string_view json, std::uint64_t shard) {
  return bitmapKey(key(json), shard);
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

class RocksDBInvertedIndexDBTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<int> counter{0};
    _path = std::filesystem::temp_directory_path() /
            ("arangodb-inverted-index-test-" + std::to_string(::getpid()) +
             "-" + std::to_string(counter++));
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

TEST(RocksDBInvertedIndexFormatTest, shards_round_trip_and_sort) {
  std::vector<std::uint64_t> shards = {0,
                                       1,
                                       255,
                                       256,
                                       65535,
                                       (1ULL << (8 * kShardBytes)) / 2 + 1,
                                       (1ULL << (8 * kShardBytes)) - 1};
  std::string const a = key(R"("a")");
  std::string previous;
  for (auto shard : shards) {
    std::string encoded = bitmapKey(a, shard);
    ASSERT_EQ(a.size() + kShardBytes, encoded.size());
    EXPECT_EQ(shard, shardOfBitmapKey(encoded, a));
    EXPECT_LT(previous, encoded);
    previous = encoded;
  }
}

TEST(RocksDBInvertedIndexFormatTest, keys_of_other_terms_have_no_shard) {
  EXPECT_FALSE(shardOfBitmapKey(key(R"("a")"), key(R"("a")")).has_value());
  EXPECT_FALSE(shardOfBitmapKey(key(R"("b")", 1), key(R"("a")")).has_value());
  EXPECT_FALSE(shardOfBitmapKey(key(R"("ab")"), key(R"("a")")).has_value());
}

TEST(RocksDBInvertedIndexFormatTest, plain_layout_splits_at_bit_31) {
  IdLayout plain{.windowBits = 0};
  std::uint64_t id = (12345ULL << 31) | 678;
  EXPECT_EQ(12345ULL, shardOf(plain, id));
  EXPECT_EQ(678U, offsetOf(plain, id));
}

namespace {
std::uint64_t hlc(std::uint64_t ms, std::uint64_t count) {
  return (ms << 20) | count;
}
}  // namespace

TEST(RocksDBInvertedIndexFormatTest, every_layout_round_trips_every_id) {
  std::vector<std::uint64_t> ids = {0,
                                    1,
                                    hlc(1'735'689'600'000, 0),
                                    hlc(1'735'689'600'000, 0xfffff),
                                    hlc(1'735'689'654'321, 17),
                                    0x0123'4567'89ab'cdefULL,
                                    ~std::uint64_t{0}};
  for (unsigned bits : {0U, 16U, 20U, 24U, 31U, 32U}) {
    IdLayout layout{.windowBits = bits};
    ASSERT_TRUE(isValid(layout));
    for (auto id : ids) {
      std::uint64_t shard = shardOf(layout, id);
      EXPECT_LT(shard, 1ULL << (8 * kShardBytes)) << bits << " " << id;
      EXPECT_EQ(id, makeId(layout, shard, offsetOf(layout, id)))
          << bits << " " << id;
    }
  }
}

TEST(RocksDBInvertedIndexFormatTest, window_layout_groups_by_window) {
  IdLayout layout{.windowBits = 20};
  std::uint64_t const start = 1'735'689'600'000ULL & ~((1ULL << 20) - 1);
  // the same window and count share a shard and a container
  EXPECT_EQ(shardOf(layout, hlc(start, 3)),
            shardOf(layout, hlc(start + 50'000, 3)));
  EXPECT_EQ(offsetOf(layout, hlc(start, 3)) >> 16,
            offsetOf(layout, hlc(start + 50'000, 3)) >> 16);
  // the next window is the next shard
  EXPECT_EQ(shardOf(layout, hlc(start, 0)) + (1ULL << 8),
            shardOf(layout, hlc(start + (1ULL << 20), 0)));
}

TEST(RocksDBInvertedIndexFormatTest, window_bits_outside_16_to_32_are_invalid) {
  EXPECT_FALSE(isValid(IdLayout{.windowBits = 15}));
  EXPECT_FALSE(isValid(IdLayout{.windowBits = 33}));
}

TEST(RocksDBInvertedIndexFormatTest,
     values_aql_considers_equal_have_equal_terms) {
  EXPECT_EQ(term("1"), term("1.0"));
  EXPECT_EQ(term("0"), term("-0.0"));
  EXPECT_NE(term("1"), term(R"("1")"));
  EXPECT_NE(term(R"("a")"), term(R"("ab")"));
  EXPECT_NE(term("true"), term("null"));
}

// The terms are part of the persisted keys. If one of these expectations
// fails, existing indexes no longer find their documents.
TEST(RocksDBInvertedIndexFormatTest, terms_have_a_fixed_encoding) {
  using namespace std::string_literals;
  EXPECT_EQ("\x00"s, term("null"));
  EXPECT_EQ("\x01"s, term("false"));
  EXPECT_EQ("\x02"s, term("true"));
  EXPECT_EQ("\x03\x3f\xf0\x00\x00\x00\x00\x00\x00"s, term("1"));
  EXPECT_EQ("\x03\xbf\xf0\x00\x00\x00\x00\x00\x00"s, term("-1"));
  EXPECT_EQ("\x03\x00\x00\x00\x00\x00\x00\x00\x00"s, term("0"));
  EXPECT_EQ("\x03\x00\x00\x00\x00\x00\x00\x00\x00"s, term("-0.0"));
  EXPECT_EQ("\x04"s, term(R"("")"));
  EXPECT_EQ(
      "\x07"
      "abc"s,
      term(R"("abc")"));
}

TEST(RocksDBInvertedIndexFormatTest, strings_switch_to_a_length_prefix) {
  auto stringTerm = [](std::size_t length) {
    velocypack::Builder b;
    b.add(velocypack::Value(std::string(length, 'x')));
    return *normalizedTerm(b.slice());
  };
  EXPECT_EQ(std::string(1, '\x7e') + std::string(122, 'x'), stringTerm(122));
  EXPECT_EQ(std::string("\x7f\x7b", 2) + std::string(123, 'x'),
            stringTerm(123));
  EXPECT_EQ(std::string("\x7f\xac\x02", 3) + std::string(300, 'x'),
            stringTerm(300));
}

TEST(RocksDBInvertedIndexFormatTest, all_nans_have_one_term) {
  velocypack::Builder quiet;
  quiet.add(velocypack::Value(std::numeric_limits<double>::quiet_NaN()));
  velocypack::Builder negative;
  negative.add(velocypack::Value(-std::numeric_limits<double>::quiet_NaN()));
  EXPECT_EQ(normalizedTerm(quiet.slice()), normalizedTerm(negative.slice()));
}

TEST(RocksDBInvertedIndexFormatTest, arrays_and_objects_have_no_term) {
  for (std::string_view json : {"[1]", R"({"a":1})", "[]"}) {
    EXPECT_FALSE(term(json).has_value()) << json;
  }
}

TEST(RocksDBInvertedIndexFormatTest,
     count_key_sorts_directly_before_bitmap_keys) {
  std::string countKey = key(R"("a")");
  std::string first = key(R"("a")", 0);
  std::string large = key(R"("a")", (1ULL << (8 * kShardBytes)) - 1);
  EXPECT_LT(countKey, first);
  EXPECT_LT(first, large);
  for (std::string_view other : {R"("b")", R"("ab")", "1", "null"}) {
    std::string otherKey = key(other);
    EXPECT_TRUE(otherKey < countKey || otherKey > large) << other;
  }
}

TEST(RocksDBInvertedIndexFormatTest, indexed_terms_are_sorted_and_distinct) {
  auto doc = velocypack::Parser::fromJson(
      R"({"a": {"labels": ["x", 1, "x", 1.0, null]}})");
  std::vector<basics::AttributeName> field;
  basics::TRI_ParseAttributeString("a.labels", field, false);
  auto terms = indexedTerms(doc->slice(), field);
  ASSERT_TRUE(terms.ok());
  std::vector<std::string> expected = {*term("1"), *term(R"("x")"),
                                       *term("null")};
  std::ranges::sort(expected);
  EXPECT_EQ(expected, terms.get());
}

TEST(RocksDBInvertedIndexFormatTest, indexed_terms_reject_non_scalar_elements) {
  auto doc = velocypack::Parser::fromJson(R"({"labels": ["x", [1]]})");
  std::vector<basics::AttributeName> field;
  basics::TRI_ParseAttributeString("labels", field, false);
  EXPECT_TRUE(indexedTerms(doc->slice(), field).fail());
}

TEST(RocksDBInvertedIndexFormatTest, documents_without_an_array_have_no_terms) {
  std::vector<basics::AttributeName> field;
  basics::TRI_ParseAttributeString("labels", field, false);
  for (std::string_view json : {R"({})", R"({"labels": "x"})"}) {
    auto doc = velocypack::Parser::fromJson(json.data(), json.size());
    auto terms = indexedTerms(doc->slice(), field);
    ASSERT_TRUE(terms.ok()) << json;
    EXPECT_TRUE(terms.get().empty()) << json;
  }
}

TEST(RocksDBInvertedIndexFormatTest, counts_round_trip) {
  EXPECT_EQ(-3, decodeCount(encodeCount(ValueTag::CountDelta, -3)));
  EXPECT_EQ(42, decodeCount(encodeCount(ValueTag::Count, 42)));
}

TEST(RocksDBInvertedIndexFormatTest,
     collector_requests_compaction_for_many_operands) {
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

TEST_F(RocksDBInvertedIndexDBTest, deltas_merge_into_a_bitmap) {
  merge("b", addDelta({1, 5}));
  merge("b", addDelta({7}));
  merge("b", removeDelta({5}));
  auto value = get("b");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ((std::vector<std::uint32_t>{1, 7}), contents(*value));
}

TEST_F(RocksDBInvertedIndexDBTest, deltas_across_flushes_merge_onto_the_base) {
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

TEST_F(RocksDBInvertedIndexDBTest, partial_merge_keeps_removes_of_older_adds) {
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

TEST_F(RocksDBInvertedIndexDBTest, counts_are_summed) {
  merge("c", encodeCount(ValueTag::CountDelta, 1));
  merge("c", encodeCount(ValueTag::CountDelta, 1));
  flush();
  merge("c", encodeCount(ValueTag::CountDelta, -1));
  merge("c", encodeCount(ValueTag::CountDelta, 3));
  auto value = get("c");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(4, decodeCount(*value));
}

TEST_F(RocksDBInvertedIndexDBTest,
       compaction_removes_empty_bitmaps_and_zero_counts) {
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

TEST_F(RocksDBInvertedIndexDBTest,
       many_memtable_operands_collapse_into_a_full_value) {
  for (std::uint32_t i = 0; i < 3 * kMaxSuccessiveMerges; ++i) {
    merge("b", addDelta({i}));
  }
  auto value = get("b");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(3 * kMaxSuccessiveMerges, contents(*value).size());
}

class RocksDBInvertedIndexPendingWritesTest
    : public RocksDBInvertedIndexDBTest {
 protected:
  RocksDBInvertedIndexPendingWritesTest()
      : _tracker(nullptr, nullptr, /*granularity*/ 1),
        _methods(&_batch, _tracker),
        _pending(_tracker) {}

  ~RocksDBInvertedIndexPendingWritesTest() override { _pending.clear(); }

  void add(std::string const& termKey, std::uint32_t offset, bool insert) {
    _pending.add(_db->DefaultColumnFamily(), termKey, /*shard*/ 0, offset,
                 insert);
  }

  /// flush into the write batch, write it and return the number of operands
  std::uint32_t write() {
    auto s = _pending.flush(_methods);
    EXPECT_TRUE(s.ok()) << s.ToString();
    std::uint32_t operands = _batch.Count();
    s = _db->Write(rocksdb::WriteOptions(), &_batch);
    EXPECT_TRUE(s.ok()) << s.ToString();
    _batch.Clear();
    _methods.resetMemoryUsage();
    return operands;
  }

  std::string bitmapKey(std::string const& termKey) {
    return inverted_index::bitmapKey(termKey, 0);
  }

  rocksdb::WriteBatch _batch;
  RocksDBMethodsMemoryTracker _tracker;
  RocksDBBatchedMethods _methods;
  PendingWrites _pending;
};

TEST_F(RocksDBInvertedIndexPendingWritesTest, writes_one_operand_per_key) {
  std::string const a = key(R"("a")");
  for (std::uint32_t i = 0; i < 1000; ++i) {
    add(a, i, true);
  }
  add(a, 7, false);
  EXPECT_EQ(2U, write());
  EXPECT_EQ(999, decodeCount(*get(a)));
  auto ids = contents(*get(bitmapKey(a)));
  EXPECT_EQ(999U, ids.size());
  EXPECT_EQ(ids.end(), std::find(ids.begin(), ids.end(), 7U));
}

TEST_F(RocksDBInvertedIndexPendingWritesTest,
       cancelled_changes_keep_only_the_remove) {
  std::string const a = key(R"("a")");
  merge(bitmapKey(a), addDelta({3}));
  add(a, 5, true);
  add(a, 5, false);
  // the count is unchanged; the remove stays, as the id may already be stored
  EXPECT_EQ(1U, write());
  EXPECT_FALSE(get(a).has_value());
  EXPECT_EQ((std::vector<std::uint32_t>{3}), contents(*get(bitmapKey(a))));
}

TEST_F(RocksDBInvertedIndexPendingWritesTest,
       rollback_undoes_changes_since_savepoint) {
  std::string const a = key(R"("a")");
  std::string const b = key(R"("b")");
  add(a, 1, true);
  add(a, 2, true);
  _pending.setSavePoint();
  add(a, 2, false);
  add(a, 3, true);
  _pending.setSavePoint();
  add(b, 4, true);
  _pending.rollbackToSavePoint();
  _pending.rollbackToSavePoint();
  _pending.setSavePoint();
  add(a, 5, true);
  _pending.popSavePoint();
  EXPECT_EQ(2U, write());
  EXPECT_EQ(3, decodeCount(*get(a)));
  EXPECT_EQ((std::vector<std::uint32_t>{1, 2, 5}),
            contents(*get(bitmapKey(a))));
  EXPECT_FALSE(get(b).has_value());
  EXPECT_FALSE(get(bitmapKey(b)).has_value());
}

namespace {
Expr termExpr(std::string_view json) {
  return Expr{.type = Expr::Type::Term, .term = *term(json)};
}
Expr constantExpr(bool value) {
  return Expr{.type = value ? Expr::Type::True : Expr::Type::False};
}
std::vector<std::string> terms(std::vector<std::string_view> jsons) {
  std::vector<std::string> result;
  for (auto json : jsons) {
    result.push_back(*term(json));
  }
  std::ranges::sort(result);
  return result;
}
}  // namespace

TEST(RocksDBInvertedIndexExprTest, fold_drops_neutral_constants) {
  Expr a = termExpr(R"("a")");
  EXPECT_EQ(a, fold(Expr{.type = Expr::Type::And,
                         .children = {a, constantExpr(true)}}));
  EXPECT_EQ(a, fold(Expr{.type = Expr::Type::Or,
                         .children = {constantExpr(false), a}}));
  EXPECT_EQ(constantExpr(false),
            fold(Expr{.type = Expr::Type::And,
                      .children = {a, constantExpr(false)}}));
  EXPECT_EQ(
      constantExpr(true),
      fold(Expr{.type = Expr::Type::Or, .children = {a, constantExpr(true)}}));
}

TEST(RocksDBInvertedIndexExprTest, complements_need_a_scan) {
  Expr notA{.type = Expr::Type::And, .excluded = {termExpr(R"("a")")}};
  EXPECT_TRUE(needsScan(fold(notA)));
  Expr notAOrB{.type = Expr::Type::Or, .children = {notA, termExpr(R"("b")")}};
  EXPECT_EQ(Expr::Type::NeedsScan, fold(notAOrB).type);
  // a required term limits the complement to the documents with that term
  Expr bAndNotA{.type = Expr::Type::And,
                .children = {termExpr(R"("b")"), notA}};
  Expr folded = fold(bAndNotA);
  EXPECT_FALSE(needsScan(folded));
  EXPECT_EQ(1U, folded.children.size());
  EXPECT_EQ(1U, folded.excluded.size());
}

TEST(RocksDBInvertedIndexExprTest, matches_evaluates_against_the_terms) {
  Expr expr{.type = Expr::Type::And,
            .children = {Expr{.type = Expr::Type::Or,
                              .children = {termExpr(R"("a")"), termExpr("1")}}},
            .excluded = {termExpr(R"("x")")}};
  EXPECT_TRUE(matches(expr, terms({R"("a")"})));
  EXPECT_TRUE(matches(expr, terms({"1.0", R"("b")"})));
  EXPECT_FALSE(matches(expr, terms({R"("a")", R"("x")"})));
  EXPECT_FALSE(matches(expr, terms({})));
}
