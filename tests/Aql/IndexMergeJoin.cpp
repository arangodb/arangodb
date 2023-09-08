#include <functional>
#include <memory>
#include <queue>
#include <span>
#include <vector>
#include <iostream>

#include <gtest/gtest.h>

#include "Logger/LogMacros.h"
#include "Basics/files.h"

#include <rocksdb/db.h>
#include <rocksdb/slice_transform.h>
#include "RocksDBEngine/RocksDBCommon.h"
#include "Basics/Endian.h"

using namespace arangodb;

namespace {
struct RocksDBInstance {
  explicit RocksDBInstance(std::string path) : _path(std::move(path)) {
    ::rocksdb::Options options;
    options.create_if_missing = true;
    options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(8));

    if (auto s = rocksdb::DB::Open(options, _path, &_db); !s.ok()) {
      auto res = arangodb::rocksutils::convertStatus(s);
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
  }

  ~RocksDBInstance() {
    delete _db;
    TRI_RemoveDirectory(_path.c_str());
  }

  [[nodiscard]] auto getDatabase() const -> rocksdb::DB* { return _db; }

 private:
  rocksdb::DB* _db = nullptr;
  std::string _path;
};
}  // namespace

struct PatternBase {
  template<typename G>
  void generateData(RocksDBInstance& db, std::size_t num, std::uint64_t prefix,
                    G&& generator) {
    static_assert(std::is_invocable_r_v<std::uint64_t, G>);

    rocksdb::WriteOptions wo;
    wo.disableWAL = true;

    std::string buffer;
    buffer.resize(16);

    prefix = basics::hostToBig(prefix);
    memcpy(buffer.data(), &prefix, 8);

    rocksdb::WriteBatch wb;
    for (std::size_t i = 0; i < num; i++) {
      auto v = basics::hostToBig<uint64_t>(generator());
      memcpy(buffer.data() + 8, &v, 8);

      auto key = rocksdb::Slice(buffer.data(), 16);
      wb.Put(key, {});

      if (wb.GetDataSize() > 10000) {
        auto s = db.getDatabase()->Write(wo, &wb);
        assert(s.Ok());
        wb.Clear();
      }
    }

    auto s = db.getDatabase()->Write(wo, &wb);
    assert(s.Ok());
  }
};

struct EvenOddPattern : PatternBase {
  void operator()(RocksDBInstance& db) {
    generateData(db, 10000000, 1, [x = 0]() mutable { return 2 * ++x; });
    generateData(db, 10000000, 2, [x = 0]() mutable { return 2 * ++x + 1; });
  }
};

struct JoinTestBase : ::testing::Test {
  static std::shared_ptr<RocksDBInstance> db;
};

std::shared_ptr<RocksDBInstance> JoinTestBase::db;

template<typename Pattern>
struct MyJoinPerformanceTest : JoinTestBase {
  static void SetUpTestCase() {
    db = std::make_shared<RocksDBInstance>("foo-bar");

    // generate some data
    Pattern{}(*db);

    rocksdb::FlushOptions fo;
    fo.wait = true;
    db->getDatabase()->Flush(fo);
  }

  static std::string buildKey(std::uint64_t prefix, std::uint64_t key) {
    std::string res;
    res.resize(16);
    auto v = basics::hostToBig(prefix);
    memcpy(res.data(), &v, 8);
    v = basics::hostToBig(prefix);
    memcpy(res.data() + 8, &v, 8);
    return res;
  }

  static void TearDownTestCase() { db.reset(); }

  static std::unique_ptr<rocksdb::Iterator> iterForPrefix(
      std::uint64_t prefix) {
    rocksdb::ReadOptions ro;
    ro.prefix_same_as_start = true;
    auto key = buildKey(prefix, 0);
    auto iter =
        std::unique_ptr<rocksdb::Iterator>(db->getDatabase()->NewIterator(ro));
    iter->Seek(key);
    return iter;
  }
};

struct SameRangePattern : PatternBase {
  void operator()(RocksDBInstance& db) {
    generateData(db, 10000000, 1, [x = 0]() mutable { return 2 * ++x; });
    generateData(db, 10000000, 2, [x = 0]() mutable { return 2 * ++x; });
  }
};

struct CommonRangePattern : PatternBase {
  void operator()(RocksDBInstance& db) {
    std::size_t total_size = 10000000;
    generateData(db, total_size, 1, [x = 0]() mutable { return ++x; });
    generateData(db, total_size, 2,
                 [x = total_size / 2]() mutable { return ++x; });
  }
};

struct HalfSize : PatternBase {
  void operator()(RocksDBInstance& db) {
    std::size_t total_size = 10000000;
    // we can assume that the optimizer would pick the smaller collection
    generateData(db, total_size / 2, 1, [x = 0]() mutable { return 2 * ++x; });
    generateData(db, total_size, 2, [x = 0]() mutable { return ++x; });
  }
};

using MyTypes = ::testing::Types<EvenOddPattern, SameRangePattern,
                                 CommonRangePattern, HalfSize>;
TYPED_TEST_SUITE(MyJoinPerformanceTest, MyTypes);

TYPED_TEST(MyJoinPerformanceTest, nested_loops_join) {
  auto iter1 = this->iterForPrefix(1);
  auto iter2 = this->iterForPrefix(2);

  std::string seekKey = this->buildKey(2, 0);

  std::size_t numSkippedSeeks = 0;
  std::size_t numSeeks = 0;
  std::size_t numResults = 0;

  while (iter1->Valid()) {
    std::uint64_t a;
    memcpy(&a, iter1->key().data_ + 8, 8);
    memcpy(seekKey.data() + 8, iter1->key().data_ + 8, 8);

    if (iter2->key().ToStringView() != seekKey) {
      iter2->Seek(seekKey);
      numSeeks += 1;
    } else {
      numSkippedSeeks += 1;
    }

    while (iter2->Valid()) {
      std::uint64_t b;
      memcpy(&b, iter2->key().data_ + 8, 8);

      if (a != b) {
        break;
      }

      // LOG_DEVEL << "a = " << basics::bigToHost(a)
      //           << " b = " << basics::bigToHost(b);
      numResults += 1;
      iter2->Next();
    }

    iter1->Next();
  }

  LOG_DEVEL << "num seeks = " << numSeeks;
  LOG_DEVEL << "num results = " << numResults;
  LOG_DEVEL << "num skipped seeks = " << numSkippedSeeks;
}

TYPED_TEST(MyJoinPerformanceTest, merge_join) {
  auto iter1 = this->iterForPrefix(1);
  auto iter2 = this->iterForPrefix(2);

  std::uint64_t iter1_prefix = basics::hostToBig<uint64_t>(1);
  std::uint64_t iter2_prefix = basics::hostToBig<uint64_t>(2);

  std::size_t numSeeks = 0;
  std::size_t numResults = 0;

  std::string seekKey;
  seekKey.resize(16);

  while (iter1->Valid() && iter2->Valid()) {
    std::uint64_t a;
    memcpy(&a, iter1->key().data_ + 8, 8);
    a = basics::bigToHost(a);

    std::uint64_t b;
    memcpy(&b, iter2->key().data_ + 8, 8);
    b = basics::bigToHost(b);

    if (a > b) {
      std::swap(iter1, iter2);
      std::swap(iter1_prefix, iter2_prefix);
      std::swap(a, b);
    } else if (a == b) {
      numResults += 1;
      iter1->Next();
      iter2->Next();
      continue;
    }

    assert(a < b);
    memcpy(seekKey.data(), &iter1_prefix, 8);
    memcpy(seekKey.data() + 8, iter2->key().data_ + 8, 8);

    numSeeks += 1;
    iter1->Seek(seekKey);
  }

  LOG_DEVEL << "num seeks = " << numSeeks;
  LOG_DEVEL << "num results = " << numResults;
}
