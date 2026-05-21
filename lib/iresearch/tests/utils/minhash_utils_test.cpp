////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "utils/minhash_utils.hpp"

#include "tests_shared.hpp"

TEST(MinHashTest, MaxSize) {
  static_assert(0 == irs::MinHash::MaxSize(1.1));
  static_assert(0 == irs::MinHash::MaxSize(1.));
  static_assert(std::numeric_limits<size_t>::max() == irs::MinHash::MaxSize(0));
  static_assert(std::numeric_limits<size_t>::max() ==
                irs::MinHash::MaxSize(-0.1));
  static_assert(100 == irs::MinHash::MaxSize(0.1));
  static_assert(400 == irs::MinHash::MaxSize(0.05));
  static_assert(1112 == irs::MinHash::MaxSize(0.03));
  static_assert(40000 == irs::MinHash::MaxSize(0.005));
}

TEST(MinHashTest, Error) {
  ASSERT_DOUBLE_EQ(std::numeric_limits<double_t>::infinity(),
                   irs::MinHash::Error(0));

  ASSERT_DOUBLE_EQ(1., irs::MinHash::Error(1));
  ASSERT_DOUBLE_EQ(0.05, irs::MinHash::Error(400));
  ASSERT_DOUBLE_EQ(0.1, irs::MinHash::Error(100));
}

TEST(MinHashTest, ConstructEmpty) {
  irs::MinHash mh{0};
  ASSERT_EQ(1, mh.MaxSize());
  ASSERT_TRUE(mh.Empty());
  ASSERT_EQ(std::begin(mh), std::end(mh));
  ASSERT_EQ(0, mh.Size());
}

TEST(MinHashTest, InsertClear) {
  constexpr size_t kNumHashes = 42;

  irs::MinHash mh{kNumHashes};
  ASSERT_EQ(kNumHashes, mh.MaxSize());
  ASSERT_EQ(0, mh.Size());
  ASSERT_TRUE(mh.Empty());
  ASSERT_EQ(std::begin(mh), std::end(mh));

  // Insert 100 values
  {
    constexpr size_t kMax = 100;

    for (size_t i = 0; i < kMax; ++i) {
      mh.Insert(i);
      const size_t size = mh.Size();

      // Duplicates are ignored
      mh.Insert(i);
      ASSERT_EQ(size, mh.Size());
    }

    ASSERT_EQ(kNumHashes, mh.Size());
    ASSERT_FALSE(mh.Empty());
    ASSERT_NE(std::begin(mh), std::end(mh));

    std::vector signature(std::begin(mh), std::end(mh));
    std::sort(std::begin(signature), std::end(signature));

    auto begin = std::begin(signature);
    for (size_t i = 0; i < kNumHashes; ++i, ++begin) {
      ASSERT_EQ(i, *begin);
    }
    ASSERT_EQ(begin, std::end(signature));
  }

  // Reset state
  mh.Clear();
  ASSERT_EQ(kNumHashes, mh.MaxSize());
  ASSERT_EQ(0, mh.Size());
  ASSERT_TRUE(mh.Empty());
  ASSERT_EQ(std::begin(mh), std::end(mh));

  // Insert 27 values
  {
    constexpr size_t kMax = 27;

    for (size_t i = 0; i < kMax; ++i) {
      mh.Insert(i);
      const size_t size = mh.Size();

      // Duplicates are ignored
      mh.Insert(i);
      ASSERT_EQ(size, mh.Size());
    }

    ASSERT_EQ(kMax, mh.Size());
    ASSERT_FALSE(mh.Empty());
    ASSERT_NE(std::begin(mh), std::end(mh));

    std::vector signature(std::begin(mh), std::end(mh));
    std::sort(std::begin(signature), std::end(signature));

    auto begin = std::begin(signature);
    for (size_t i = 0; i < kMax; ++i, ++begin) {
      ASSERT_EQ(i, *begin);
    }
    ASSERT_EQ(begin, std::end(signature));
  }
}

TEST(MinHashTest, Jaccard) {
  auto make = [](size_t n) {
    irs::MinHash min_hash{n};
    for (size_t i = 0; i < min_hash.MaxSize(); ++i) {
      min_hash.Insert(i);
    }
    return min_hash;
  };

  auto assert_jaccard = [](const irs::MinHash& lhs, const irs::MinHash rhs,
                           double expected) {
    // Circumvent Apple Clang build issue
    const std::vector<uint64_t> rhs_values{std::begin(rhs), std::end(rhs)};
    const std::vector<uint64_t> lhs_values{std::begin(lhs), std::end(lhs)};

    ASSERT_DOUBLE_EQ(expected, lhs.Jaccard(rhs_values));
    ASSERT_DOUBLE_EQ(expected, rhs.Jaccard(lhs_values));
    ASSERT_DOUBLE_EQ(expected, lhs.Jaccard(rhs));
    ASSERT_DOUBLE_EQ(expected, rhs.Jaccard(lhs));
  };

  assert_jaccard(irs::MinHash{0}, irs::MinHash{0}, 1.);
  assert_jaccard(irs::MinHash{0}, make(42), 0.);
  assert_jaccard(make(21), make(42), 0.5);
  assert_jaccard(make(42), make(42), 1.);
  assert_jaccard(make(1), make(42), 1. / 42);
  assert_jaccard(make(100), make(42), 42. / 100);
}
