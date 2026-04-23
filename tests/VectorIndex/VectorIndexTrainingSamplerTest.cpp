////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndex/VectorIndexTrainingSampler.h"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::vector;

namespace {

// Feeds one vector through the sampler. Mirrors the production caller: only
// decode into the buffer when the sampler wants the item; otherwise advance
// the seen-counter via skip().
void feed(VectorIndexTrainingSampler& s, std::vector<float> const& vector) {
  if (s.wantsItem()) {
    auto& buf = s.inputBuffer();
    buf.insert(buf.end(), vector.begin(), vector.end());
    s.consume();
  } else {
    s.skip();
  }
}

// Builds a 1-D vector holding the single float `v`. Handy for tests that
// identify items by a scalar id.
std::vector<float> scalar(float v) { return {v}; }

// Returns the i-th slot (a `dimension`-sized span starting at i * dimension)
// as a vector.
std::vector<float> slotAt(std::vector<float> const& reservoir,
                          std::size_t dimension, std::size_t i) {
  return std::vector<float>(reservoir.begin() + i * dimension,
                            reservoir.begin() + (i + 1) * dimension);
}

constexpr std::uint64_t kFixedSeed = 0xABCDEF0123456789ULL;

}  // namespace

// -----------------------------------------------------------------------------
// Fill phase
// -----------------------------------------------------------------------------

TEST(VectorIndexTrainingSamplerTest, below_capacity_keeps_everything) {
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/10, kFixedSeed};
  for (int i = 0; i < 5; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  EXPECT_EQ(s.itemsSeen(), 5u);
  auto data = std::move(s).release();
  ASSERT_EQ(data.size(), 5u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(data[i], static_cast<float>(i));
  }
}

TEST(VectorIndexTrainingSamplerTest, at_capacity_keeps_everything) {
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/5, kFixedSeed};
  for (int i = 0; i < 5; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  EXPECT_EQ(s.itemsSeen(), 5u);
  auto data = std::move(s).release();
  ASSERT_EQ(data.size(), 5u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(data[i], static_cast<float>(i));
  }
}

// -----------------------------------------------------------------------------
// Replace phase
// -----------------------------------------------------------------------------

TEST(VectorIndexTrainingSamplerTest, over_capacity_size_matches_capacity) {
  constexpr std::size_t k = 10;
  constexpr std::size_t n = 1000;
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, kFixedSeed};
  for (std::size_t i = 0; i < n; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  EXPECT_EQ(s.itemsSeen(), n);
  auto data = std::move(s).release();
  EXPECT_EQ(data.size(), k);
}

TEST(VectorIndexTrainingSamplerTest, reservoir_is_subset_of_distinct_inputs) {
  // Feed N distinct scalar inputs; reservoir must contain k distinct values,
  // each drawn from the input stream.
  constexpr std::size_t k = 16;
  constexpr std::size_t n = 500;
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, kFixedSeed};
  for (std::size_t i = 0; i < n; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  auto data = std::move(s).release();
  ASSERT_EQ(data.size(), k);

  std::unordered_set<int> kept;
  for (float f : data) {
    int const idx = static_cast<int>(f);
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, static_cast<int>(n));
    kept.insert(idx);
  }
  EXPECT_EQ(kept.size(), k) << "reservoir contains duplicate slots";
}

// -----------------------------------------------------------------------------
// Multi-dimensional slot layout
// -----------------------------------------------------------------------------

TEST(VectorIndexTrainingSamplerTest, multi_dimensional_slots_are_contiguous) {
  constexpr std::size_t dim = 4;
  constexpr std::size_t k = 8;
  constexpr std::size_t n = 200;
  VectorIndexTrainingSampler s{dim, /*capacity=*/k, kFixedSeed};

  // Feed items where each i maps to the vector (i, i+1, i+2, i+3) so we can
  // identify which input produced a slot.
  for (std::size_t i = 0; i < n; ++i) {
    std::vector<float> v(dim);
    for (std::size_t d = 0; d < dim; ++d) {
      v[d] = static_cast<float>(i + d);
    }
    feed(s, v);
  }
  auto data = std::move(s).release();
  ASSERT_EQ(data.size(), k * dim);

  for (std::size_t i = 0; i < k; ++i) {
    auto slot = slotAt(data, dim, i);
    ASSERT_EQ(slot.size(), dim);
    float const base = slot[0];
    for (std::size_t d = 0; d < dim; ++d) {
      EXPECT_EQ(slot[d], base + static_cast<float>(d))
          << "slot " << i << " at dim " << d << " is not a contiguous input";
    }
  }
}

// -----------------------------------------------------------------------------
// Determinism
// -----------------------------------------------------------------------------

TEST(VectorIndexTrainingSamplerTest, fixed_seed_is_deterministic) {
  constexpr std::size_t k = 32;
  constexpr std::size_t n = 1000;
  auto run = [&] {
    VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, kFixedSeed};
    for (std::size_t i = 0; i < n; ++i) {
      feed(s, scalar(static_cast<float>(i)));
    }
    return std::move(s).release();
  };
  EXPECT_EQ(run(), run());
}

TEST(VectorIndexTrainingSamplerTest, different_seeds_diverge) {
  constexpr std::size_t k = 32;
  constexpr std::size_t n = 1000;
  auto run = [&](std::uint64_t seed) {
    VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, seed};
    for (std::size_t i = 0; i < n; ++i) {
      feed(s, scalar(static_cast<float>(i)));
    }
    return std::move(s).release();
  };
  auto a = run(1);
  auto b = run(2);
  EXPECT_NE(a, b);
}

// -----------------------------------------------------------------------------
// resize()
// -----------------------------------------------------------------------------

TEST(VectorIndexTrainingSamplerTest, resize_smaller_shrinks_reservoir) {
  constexpr std::size_t k = 20;
  constexpr std::size_t n = 200;
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, kFixedSeed};
  for (std::size_t i = 0; i < n; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  s.resize(5);
  auto data = std::move(s).release();
  EXPECT_EQ(data.size(), 5u);

  // Every value must still come from the input stream.
  for (float f : data) {
    auto const idx = static_cast<int>(f);
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, static_cast<int>(n));
  }
}

TEST(VectorIndexTrainingSamplerTest, resize_larger_is_noop) {
  constexpr std::size_t k = 10;
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, kFixedSeed};
  for (std::size_t i = 0; i < k; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  s.resize(100);  // larger than current size
  auto data = std::move(s).release();
  ASSERT_EQ(data.size(), k);
  for (std::size_t i = 0; i < k; ++i) {
    EXPECT_EQ(data[i], static_cast<float>(i));
  }
}

TEST(VectorIndexTrainingSamplerTest, resize_on_undersized_reservoir_is_noop) {
  // Reservoir never filled past 5 items; resize(10) should leave all 5 in
  // place (new capacity >= current size).
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/20, kFixedSeed};
  for (std::size_t i = 0; i < 5; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  s.resize(10);
  auto data = std::move(s).release();
  ASSERT_EQ(data.size(), 5u);
  for (std::size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(data[i], static_cast<float>(i));
  }
}

// -----------------------------------------------------------------------------
// inputBuffer() semantics
// -----------------------------------------------------------------------------

TEST(VectorIndexTrainingSamplerTest, input_buffer_is_cleared_each_call) {
  VectorIndexTrainingSampler s{/*dimension=*/3, /*capacity=*/4, kFixedSeed};
  auto& a = s.inputBuffer();
  EXPECT_TRUE(a.empty());
  a.push_back(1.0f);
  a.push_back(2.0f);
  // Request the buffer again without consume — must come back empty.
  auto& b = s.inputBuffer();
  EXPECT_TRUE(b.empty());
  EXPECT_EQ(&a, &b) << "inputBuffer() should return a stable reference";
}

TEST(VectorIndexTrainingSamplerTest, itemsSeen_counts_every_consumed_vector) {
  // itemsSeen must count every consumed vector regardless of whether
  // Algorithm L kept or evicted it.
  constexpr std::size_t k = 5;
  constexpr std::size_t n = 123;
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, kFixedSeed};
  for (std::size_t i = 0; i < n; ++i) {
    feed(s, scalar(static_cast<float>(i)));
  }
  EXPECT_EQ(s.itemsSeen(), n);
}

// -----------------------------------------------------------------------------
// wantsItem() / skip() semantics
// -----------------------------------------------------------------------------

TEST(VectorIndexTrainingSamplerTest, wants_item_true_through_fill_phase) {
  // Every position in the fill phase must request the item; otherwise the
  // reservoir would end up short and Algorithm L's invariant would break.
  constexpr std::size_t k = 8;
  VectorIndexTrainingSampler s{/*dimension=*/1, /*capacity=*/k, kFixedSeed};
  for (std::size_t i = 0; i < k; ++i) {
    EXPECT_TRUE(s.wantsItem()) << "wantsItem() false at fill index " << i;
    feed(s, scalar(static_cast<float>(i)));
  }
}

TEST(VectorIndexTrainingSamplerTest,
     wants_item_decides_whether_skip_or_consume_runs) {
  // Past capacity, wantsItem() must be true exactly at positions where
  // feed() actually mutates the reservoir, and false everywhere else.
  constexpr std::size_t dim = 1;
  constexpr std::size_t k = 4;
  constexpr std::size_t n = 200;
  VectorIndexTrainingSampler s{dim, k, kFixedSeed};

  std::size_t consumed = 0;
  std::size_t skipped = 0;
  for (std::size_t i = 0; i < n; ++i) {
    bool const wanted = s.wantsItem();
    if (wanted) {
      auto& buf = s.inputBuffer();
      buf.push_back(static_cast<float>(i));
      s.consume();
      ++consumed;
    } else {
      s.skip();
      ++skipped;
    }
  }
  EXPECT_EQ(consumed + skipped, n);
  EXPECT_EQ(s.itemsSeen(), n);
  // Past the fill phase the replacement schedule is sparse, so most items
  // must have been skipped rather than consumed.
  EXPECT_GT(skipped, consumed);
}
