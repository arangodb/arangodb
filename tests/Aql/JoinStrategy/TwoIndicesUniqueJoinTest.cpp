#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Logger/LogMacros.h"

#include "Aql/IndexJoin/TwoIndicesUniqueMergeJoin.h"

using namespace arangodb;

namespace {
using MyKeyValue = std::size_t;
using MyDocumentId = std::size_t;

using Strategy =
    arangodb::aql::TwoIndicesUniqueMergeJoin<MyKeyValue, MyDocumentId>;
using MyIndexStreamIterator = Strategy::StreamIteratorType;

using Desc = Strategy::Descriptor;

struct MyVectorIterator : MyIndexStreamIterator {
  bool position(std::span<MyKeyValue> sp) const override {
    if (current != end) {
      sp[0] = *current;
      return true;
    }
    return false;
  }

  bool seek(std::span<MyKeyValue> key) override {
    current = std::lower_bound(
        begin, end, key[0], [](auto left, auto right) { return left < right; });
    if (current != end) {
      key[0] = *current;
      return true;
    }
    return false;
  }

  MyDocumentId load(std::span<MyKeyValue> projections) const override {
    return *current;
  }

  void cacheCurrentKey(std::span<MyKeyValue> cache) override {
    cache[0] = *current;
  }

  bool reset(std::span<MyKeyValue> span) override {
    current = begin;
    if (current != end) {
      span[0] = *current;
    }
    return current != end;
  }

  bool next(std::span<MyKeyValue> key, MyDocumentId& doc,
            std::span<MyKeyValue> projections) override {
    current = current + 1;
    if (current == end) {
      return false;
    }

    key[0] = *current;
    doc = *current;
    return true;
  }

  using Container = std::span<MyKeyValue>;

  Container::iterator begin;
  Container::iterator current;
  Container::iterator end;

  MyVectorIterator(Container c)
      : begin(c.begin()), current(begin), end(c.end()) {}
};

}  // namespace

enum class ReadMore { YES, NO };

class UniqueIndexMerger : public ::testing::TestWithParam<ReadMore> {
 protected:
  UniqueIndexMerger() = default;

  [[nodiscard]] bool doReadMore() { return GetParam() == ReadMore::YES; }
};

INSTANTIATE_TEST_CASE_P(UniqueIndexMergerTestRunner, UniqueIndexMerger,
                        ::testing::Values(ReadMore::YES, ReadMore::NO));

TEST_P(UniqueIndexMerger, no_results) {
  bool isUnique = true;
  std::vector<MyKeyValue> a = {
      1,
      3,
      5,
      7,
  };

  std::vector<MyKeyValue> b = {
      2, 4, 6, 8, 10,
  };

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  Strategy merger{std::move(iters), 1};

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    hasMore =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          count += 1;
          return doReadMore();
        });
  }

  ASSERT_EQ(count, 0);
}

TEST_P(UniqueIndexMerger, some_results_a) {
  bool isUnique = true;
  std::vector<MyKeyValue> a = {
      1, 3, 5, 7, 8, 9,
  };

  std::vector<MyKeyValue> b = {
      2, 4, 6, 8, 10,
  };

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  Strategy merger{std::move(iters), 1};

  bool hasMore = true;
  std::size_t count = 0;

  while (hasMore) {
    hasMore =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);

          count += 1;
          return doReadMore();
        });
  }

  ASSERT_EQ(count, 1);
}

TEST_P(UniqueIndexMerger, some_results_b) {
  bool isUnique = true;
  std::vector<MyKeyValue> a = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  };

  std::vector<MyKeyValue> b = {
      2, 4, 6, 8, 10,
  };

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  Strategy merger{std::move(iters), 1};

  bool hasMore = true;
  std::size_t count = 0;

  while (hasMore) {
    hasMore =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);

          count += 1;
          return doReadMore();
        });
  }

  ASSERT_EQ(count, 5);
}

TEST_P(UniqueIndexMerger, one_empty) {
  bool isUnique = true;
  std::vector<MyKeyValue> a = {};

  std::vector<MyKeyValue> b = {
      2, 4, 6, 8, 10,
  };

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  Strategy merger{std::move(iters), 1};

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    hasMore =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
  }

  ASSERT_EQ(count, 0);
}

TEST_P(UniqueIndexMerger, both_empty) {
  bool isUnique = true;
  std::vector<MyKeyValue> a = {};

  std::vector<MyKeyValue> b = {};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  Strategy merger{std::move(iters), 1};

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    hasMore =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
  }

  ASSERT_EQ(count, 0);
}