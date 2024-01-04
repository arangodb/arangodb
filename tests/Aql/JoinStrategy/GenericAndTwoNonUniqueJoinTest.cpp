#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Logger/LogMacros.h"

#include "Aql/IndexJoin/GenericMerge.h"
#include "Aql/IndexJoin/TwoIndicesMergeJoin.h"

using namespace arangodb;

namespace {
using MyKeyValue = std::size_t;
using MyDocumentId = std::size_t;

using GenericJoinStrategy =
    arangodb::aql::GenericMergeJoin<MyKeyValue, MyDocumentId>;
using TwoIndexNonUniqueJoinStrategy =
    arangodb::aql::TwoIndicesMergeJoin<MyKeyValue, MyDocumentId>;
using MyIndexStreamIterator = GenericJoinStrategy::StreamIteratorType;

using Desc = GenericJoinStrategy::Descriptor;

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

  bool reset(std::span<MyKeyValue> span,
             std::span<MyKeyValue> constants) override {
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

struct TestConfiguration {
  ReadMore _readMore;
  std::string _strategyName;
};

// Covers all tests which can be executed based on the Generic Join
// Strategy and the Two Indices (non-unique) Join Strategy
class GenericAndTwoNonUniqueIndexMerger
    : public ::testing::TestWithParam<TestConfiguration> {
 protected:
  GenericAndTwoNonUniqueIndexMerger() = default;

  [[nodiscard]] static bool doReadMore() {
    return GetParam()._readMore == ReadMore::YES;
  }

  [[nodiscard]] static std::string_view getStrategyName() {
    return GetParam()._strategyName;
  }

  [[nodiscard]] static std::unique_ptr<
      arangodb::aql::IndexJoinStrategy<MyKeyValue, MyDocumentId>>
  buildStrategy(std::vector<Desc>&& iters) {
    for (auto& desc : iters) {
      desc.numKeyComponents = {1};
      desc.numConstants = {0};
    }

    if (getStrategyName() == "GenericJoin") {
      auto strategy = std::make_unique<GenericJoinStrategy>(std::move(iters));
      strategy.get()->reset({});
      return strategy;
    } else {
      TRI_ASSERT(getStrategyName() == "TwoIndicesMergeJoin");
      auto strategy =
          std::make_unique<TwoIndexNonUniqueJoinStrategy>(std::move(iters));
      strategy.get()->reset({});
      return strategy;
    }
  }
};

// Covers all tests which can only be executed based on the Generic Join
// Strategy
class GenericIndexMerger : public ::testing::TestWithParam<ReadMore> {
 protected:
  GenericIndexMerger() = default;
  [[nodiscard]] static bool doReadMore() { return GetParam() == ReadMore::YES; }
};

INSTANTIATE_TEST_CASE_P(
    IndexMergerTestRunner, GenericAndTwoNonUniqueIndexMerger,
    ::testing::Values(TestConfiguration{._readMore = ReadMore::YES,
                                        ._strategyName = "GenericJoin"},
                      TestConfiguration{._readMore = ReadMore::NO,
                                        ._strategyName = "GenericJoin"},
                      TestConfiguration{._readMore = ReadMore::YES,
                                        ._strategyName = "TwoIndicesMergeJoin"},
                      TestConfiguration{
                          ._readMore = ReadMore::NO,
                          ._strategyName = "TwoIndicesMergeJoin"}));

INSTANTIATE_TEST_CASE_P(GenericMergerTestRunner, GenericIndexMerger,
                        ::testing::Values(ReadMore::YES, ReadMore::NO));

TEST_P(GenericAndTwoNonUniqueIndexMerger, no_results) {
  bool isUnique = false;
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

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 0);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger, small_result_test_seeks) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {
      1,
      2,
  };

  std::vector<MyKeyValue> b = {2};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    ASSERT_EQ(amountOfSeeks, 1);
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 1);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger, some_results) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {
      1, 3, 5, 7, 8, 9,
  };

  std::vector<MyKeyValue> b = {
      2, 4, 6, 8, 10,
  };

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    ASSERT_TRUE(amountOfSeeks >= 1);
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 1);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger, one_empty) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {};

  std::vector<MyKeyValue> b = {
      2, 4, 6, 8, 10,
  };

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 0);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger, both_empty) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {};

  std::vector<MyKeyValue> b = {};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 0);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger, product_result) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {1, 1};
  std::vector<MyKeyValue> b = {1, 1};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 4);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger, two_phase_product_result) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {1, 1, 3, 4};

  std::vector<MyKeyValue> b = {1, 1, 2, 4};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 5);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger,
       two_phase_product_result_two_streaks) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {1, 1, 2, 2};

  std::vector<MyKeyValue> b = {1, 1, 2, 2};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 4 + 4);
}

TEST_P(GenericAndTwoNonUniqueIndexMerger,
       two_phase_product_result_two_streaks_x) {
  bool isUnique = false;

  std::vector<MyKeyValue> a = {1, 1, 2, 2, 3, 4, 8};

  std::vector<MyKeyValue> b = {1, 1, 2, 2, 5, 6, 8};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);

  auto merger = buildStrategy(std::move(iters));

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger->next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs[0], docs[1]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 4 + 4 + 1);
}

TEST_P(GenericIndexMerger, one_iterator_corner_case) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {0, 1, 2, 3};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  for (auto& desc : iters) {
    desc.numKeyComponents = 1;
    desc.numConstants = 0;
  }

  // cannot be executed with the two indices join strategy (checked and
  // asserted). Therefore, only tested in the generic case.
  GenericJoinStrategy merger{std::move(iters)};
  merger.reset({});

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs.size(), 1);
          EXPECT_EQ(docs[0], count);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, a.size());
}

TEST_P(GenericIndexMerger, three_iterators) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {
      1, 1, 3, 4, 6, 7, 8, 9,
  };

  std::vector<MyKeyValue> b = {
      1, 1, 2, 4, 6, 7, 8, 10,
  };

  std::vector<MyKeyValue> c = {
      2, 2, 2, 4, 7, 8, 10,
  };

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(c), 0, isUnique);
  for (auto& desc : iters) {
    desc.numKeyComponents = 1;
    desc.numConstants = 0;
  }

  GenericJoinStrategy merger{std::move(iters)};
  merger.reset({});

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs.size(), 3);
          LOG_DEVEL << docs[0] << " - " << docs[1] << " - " << docs[2];
          EXPECT_EQ(docs[0], docs[1]);
          EXPECT_EQ(docs[1], docs[2]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 3);
}

TEST_P(GenericIndexMerger, three_iterators_2) {
  bool isUnique = false;
  std::vector<MyKeyValue> a = {
      1,
      2,
      3,
  };

  std::vector<MyKeyValue> b = {0, 2, 2, 4};

  std::vector<MyKeyValue> c = {0, 2, 5};

  std::vector<Desc> iters;
  iters.emplace_back(std::make_unique<MyVectorIterator>(a), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(b), 0, isUnique);
  iters.emplace_back(std::make_unique<MyVectorIterator>(c), 0, isUnique);
  for (auto& desc : iters) {
    desc.numKeyComponents = 1;
    desc.numConstants = 0;
  }

  GenericJoinStrategy merger{std::move(iters)};
  merger.reset({});

  bool hasMore = true;
  std::size_t count = 0;
  while (hasMore) {
    auto [hasMoreNext, amountOfSeeks] =
        merger.next([&](std::span<MyDocumentId> docs, std::span<MyKeyValue>) {
          EXPECT_EQ(docs.size(), 3);
          LOG_DEVEL << docs[0] << " - " << docs[1] << " - " << docs[2];
          EXPECT_EQ(docs[0], docs[1]);
          EXPECT_EQ(docs[1], docs[2]);
          count += 1;
          return doReadMore();
        });
    hasMore = hasMoreNext;
  }

  ASSERT_EQ(count, 2);
}