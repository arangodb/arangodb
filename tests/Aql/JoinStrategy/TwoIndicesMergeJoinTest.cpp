#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Logger/LogMacros.h"

#include "Aql/IndexJoin/TwoIndicesMergeJoin.h"

using namespace arangodb;

namespace {
using MyKeyValue = std::size_t;
using MyDocumentId = std::size_t;

using Strategy = arangodb::aql::TwoIndicesMergeJoin<MyKeyValue, MyDocumentId>;
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

class TwoIndexMerger : public ::testing::TestWithParam<ReadMore> {
 protected:
  TwoIndexMerger() = default;

  [[nodiscard]] bool doReadMore() { return GetParam() == ReadMore::YES; }
};

INSTANTIATE_TEST_CASE_P(TwoIndexMergerTestRunner, TwoIndexMerger,
                        ::testing::Values(ReadMore::YES));

TEST_P(TwoIndexMerger, some_results_a) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {1, 2};
  std::vector<MyKeyValue> b = {1, 2, 2};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  Strategy merger{std::move(iters), 1};

  bool hasMore = true;
  std::size_t count = 0;
  std::vector<MyDocumentId> expectedResult = {1, 2, 2};
  size_t totalAmountOfSeeks = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          LOG_DEVEL << "docs[0] = " << docs[0];
          LOG_DEVEL << "docs[1] = " << docs[1];
          LOG_DEVEL << "expectedResult[count] = " << expectedResult.at(count);
          EXPECT_LT(count, expectedResult.size());
          EXPECT_EQ(docs[0], expectedResult[count]);
          count += 1;
          return doReadMore();
        });
    totalAmountOfSeeks += amountOfSeeks;
    hasMore = hasMoreNext;
  }
  ASSERT_TRUE(totalAmountOfSeeks >= 1);
  LOG_DEVEL << "Total amount of seeks after finish: " << totalAmountOfSeeks;
  ASSERT_EQ(count, 3);
}

TEST_P(TwoIndexMerger, some_results_b) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {1, 2, 3, 4, 5, 5, 5};
  std::vector<MyKeyValue> b = {1, 4, 5, 5, 6, 7, 8, 9, 10};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  Strategy merger{std::move(iters), 1};

  bool hasMore = true;
  std::size_t count = 0;
  std::vector<MyDocumentId> expectedResult = {1, 4, 5, 5, 5, 5, 5, 5};
  size_t totalAmountOfSeeks = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          LOG_DEVEL << "docs[0] = " << docs[0];
          LOG_DEVEL << "docs[1] = " << docs[1];
          LOG_DEVEL << "expectedResult[count] = " << expectedResult.at(count);
          EXPECT_LT(count, expectedResult.size());
          EXPECT_EQ(docs[0], expectedResult[count]);
          count += 1;
          return doReadMore();
        });
    totalAmountOfSeeks += amountOfSeeks;
    hasMore = hasMoreNext;
  }
  ASSERT_TRUE(totalAmountOfSeeks >= 1);
  LOG_DEVEL << "Total amount of seeks after finish: " << totalAmountOfSeeks;
  ASSERT_EQ(count, expectedResult.size());
}