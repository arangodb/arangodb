// Copyright 2021 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS  // before inttypes.h
#endif
#include <inttypes.h>  // IWYU pragma: keep
#include <stdio.h>
#include <string.h>  // memcpy

#include <unordered_map>
#include <vector>

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/sort_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/contrib/sort/vqsort.h"
// After foreach_target
#include "hwy/contrib/sort/algo-inl.h"
#include "hwy/contrib/sort/traits128-inl.h"
#include "hwy/contrib/sort/result-inl.h"
#include "hwy/contrib/sort/vqsort-inl.h"  // BaseCase
#include "hwy/tests/test_util-inl.h"
// clang-format on

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

using detail::OrderAscending;
using detail::SharedTraits;
using detail::TraitsLane;
#if VQSORT_ENABLED || HWY_IDE
#if !HAVE_INTEL
using detail::OrderAscending128;
using detail::OrderAscendingKV128;
using detail::OrderAscendingKV64;
using detail::OrderDescending128;
using detail::OrderDescendingKV128;
using detail::OrderDescendingKV64;
using detail::Traits128;
#endif

template <class Traits>
static HWY_NOINLINE void TestMedian3() {
  using LaneType = typename Traits::LaneType;
  using D = CappedTag<LaneType, 1>;
  SharedTraits<Traits> st;
  const D d;
  using V = Vec<D>;
  for (uint32_t bits = 0; bits < 8; ++bits) {
    const V v0 = Set(d, LaneType{(bits & (1u << 0)) ? 1u : 0u});
    const V v1 = Set(d, LaneType{(bits & (1u << 1)) ? 1u : 0u});
    const V v2 = Set(d, LaneType{(bits & (1u << 2)) ? 1u : 0u});
    const LaneType m = GetLane(detail::MedianOf3(st, v0, v1, v2));
    // If at least half(rounded up) of bits are 1, so is the median.
    const size_t count = PopCount(bits);
    HWY_ASSERT_EQ((count >= 2) ? static_cast<LaneType>(1) : 0, m);
  }
}

HWY_NOINLINE void TestAllMedian() {
  TestMedian3<TraitsLane<OrderAscending<uint64_t> > >();
}

template <class Traits>
static HWY_NOINLINE void TestBaseCaseAscDesc() {
  using LaneType = typename Traits::LaneType;
  SharedTraits<Traits> st;
  const SortTag<LaneType> d;
  const size_t N = Lanes(d);
  constexpr size_t N1 = st.LanesPerKey();
  const size_t base_case_num = SortConstants::BaseCaseNumLanes<N1>(N);

  constexpr int kDebug = 0;
  auto aligned_lanes = hwy::AllocateAligned<LaneType>(N + base_case_num + N);
  auto buf = hwy::AllocateAligned<LaneType>(base_case_num + 2 * N);
  HWY_ASSERT(aligned_lanes && buf);

  std::vector<size_t> lengths;
  lengths.push_back(HWY_MAX(1, N1));
  lengths.push_back(3 * N1);
  lengths.push_back(base_case_num / 2);
  lengths.push_back(base_case_num / 2 + N1);
  lengths.push_back(base_case_num - N1);
  lengths.push_back(base_case_num);

  std::vector<size_t> misalignments;
  misalignments.push_back(0);
  misalignments.push_back(1);
  if (N >= 6) misalignments.push_back(N / 2 - 1);
  misalignments.push_back(N / 2);
  misalignments.push_back(N / 2 + 1);
  misalignments.push_back(HWY_MIN(2 * N / 3 + 3, size_t{N - 1}));

  for (bool asc : {false, true}) {
    for (size_t len : lengths) {
      for (size_t misalign : misalignments) {
        LaneType* HWY_RESTRICT lanes = aligned_lanes.get() + misalign;
        if (kDebug) {
          printf("============%s asc %d N1 %d len %d misalign %d\n",
                 st.KeyString(), asc, static_cast<int>(N1),
                 static_cast<int>(len), static_cast<int>(misalign));
        }

        for (size_t i = 0; i < misalign; ++i) {
          aligned_lanes[i] = hwy::LowestValue<LaneType>();
        }
        InputStats<LaneType> input_stats;
        for (size_t i = 0; i < len; ++i) {
          lanes[i] = asc ? static_cast<LaneType>(LaneType(i) + 1)
                         : static_cast<LaneType>(LaneType(len) - LaneType(i));
          input_stats.Notify(lanes[i]);
          if (kDebug >= 2) {
            printf("%3zu: %f\n", i, static_cast<double>(lanes[i]));
          }
        }
        for (size_t i = len; i < base_case_num + N; ++i) {
          lanes[i] = hwy::LowestValue<LaneType>();
        }

        detail::BaseCase(d, st, lanes, len, buf.get());

        if (kDebug >= 2) {
          printf("out>>>>>>\n");
          for (size_t i = 0; i < len; ++i) {
            printf("%3zu: %f\n", i, static_cast<double>(lanes[i]));
          }
        }

        HWY_ASSERT(VerifySort(st, input_stats, lanes, len, "BaseAscDesc"));
        for (size_t i = 0; i < misalign; ++i) {
          if (aligned_lanes[i] != hwy::LowestValue<LaneType>())
            HWY_ABORT("Overrun misalign at %d\n", static_cast<int>(i));
        }
        for (size_t i = len; i < base_case_num + N; ++i) {
          if (lanes[i] != hwy::LowestValue<LaneType>())
            HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
        }
      }  // misalign
    }    // len
  }      // asc
}

template <class Traits>
static HWY_NOINLINE void TestBaseCase01() {
  using LaneType = typename Traits::LaneType;
  SharedTraits<Traits> st;
  const SortTag<LaneType> d;
  const size_t N = Lanes(d);
  constexpr size_t N1 = st.LanesPerKey();
  const size_t base_case_num = SortConstants::BaseCaseNumLanes<N1>(N);

  constexpr int kDebug = 0;
  auto lanes = hwy::AllocateAligned<LaneType>(base_case_num + N);
  auto buf = hwy::AllocateAligned<LaneType>(base_case_num + 2 * N);
  HWY_ASSERT(lanes && buf);

  std::vector<size_t> lengths;
  lengths.push_back(HWY_MAX(1, N1));
  lengths.push_back(3 * N1);
  lengths.push_back(base_case_num / 2);
  lengths.push_back(base_case_num / 2 + N1);
  lengths.push_back(base_case_num - N1);
  lengths.push_back(base_case_num);

  for (size_t len : lengths) {
    if (kDebug) {
      printf("============%s 01 N1 %d len %d\n", st.KeyString(),
             static_cast<int>(N1), static_cast<int>(len));
    }
    const uint64_t kMaxBits = AdjustedLog2Reps(HWY_MIN(len, size_t{14}));
    for (uint64_t bits = 0; bits < ((1ull << kMaxBits) - 1); ++bits) {
      InputStats<LaneType> input_stats;
      for (size_t i = 0; i < len; ++i) {
        lanes[i] = (i < 64 && (bits & (1ull << i))) ? 1 : 0;
        input_stats.Notify(lanes[i]);
        if (kDebug >= 2) {
          printf("%3zu: %f\n", i, static_cast<double>(lanes[i]));
        }
      }
      for (size_t i = len; i < base_case_num + N; ++i) {
        lanes[i] = hwy::LowestValue<LaneType>();
      }

      detail::BaseCase(d, st, lanes.get(), len, buf.get());

      if (kDebug >= 2) {
        printf("out>>>>>>\n");
        for (size_t i = 0; i < len; ++i) {
          printf("%3zu: %f\n", i, static_cast<double>(lanes[i]));
        }
      }

      HWY_ASSERT(VerifySort(st, input_stats, lanes.get(), len, "Base01"));
      for (size_t i = len; i < base_case_num + N; ++i) {
        if (lanes[i] != hwy::LowestValue<LaneType>())
          HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
      }
    }  // bits
  }    // len
}

template <class Traits>
static HWY_NOINLINE void TestBaseCase() {
  TestBaseCaseAscDesc<Traits>();
  TestBaseCase01<Traits>();
}

HWY_NOINLINE void TestAllBaseCase() {
  // Workaround for stack overflow on MSVC debug.
#if defined(_MSC_VER)
  return;
#endif
  TestBaseCase<TraitsLane<OrderAscending<int32_t> > >();
  TestBaseCase<TraitsLane<OtherOrder<int64_t> > >();
#if !HAVE_INTEL
  TestBaseCase<Traits128<OrderAscending128> >();
  TestBaseCase<Traits128<OrderDescending128> >();
#endif
}

template <class Traits>
static HWY_NOINLINE void VerifyPartition(
    Traits st, typename Traits::LaneType* HWY_RESTRICT lanes, size_t left,
    size_t border, size_t right, const size_t N1,
    const typename Traits::LaneType* pivot) {
  /* for (size_t i = left; i < right; ++i) {
     if (i == border) printf("--\n");
     printf("%4zu: %3d\n", i, lanes[i]);
   }*/

  HWY_ASSERT(left % N1 == 0);
  HWY_ASSERT(border % N1 == 0);
  HWY_ASSERT(right % N1 == 0);
  const bool asc = typename Traits::Order().IsAscending();
  for (size_t i = left; i < border; i += N1) {
    if (st.Compare1(pivot, lanes + i)) {
      HWY_ABORT(
          "%s: asc %d left[%d] piv %.0f %.0f compares before %.0f %.0f "
          "border %d",
          st.KeyString(), asc, static_cast<int>(i),
          static_cast<double>(pivot[1]), static_cast<double>(pivot[0]),
          static_cast<double>(lanes[i + 1]), static_cast<double>(lanes[i + 0]),
          static_cast<int>(border));
    }
  }
  for (size_t i = border; i < right; i += N1) {
    if (!st.Compare1(pivot, lanes + i)) {
      HWY_ABORT(
          "%s: asc %d right[%d] piv %.0f %.0f compares after %.0f %.0f "
          "border %d",
          st.KeyString(), asc, static_cast<int>(i),
          static_cast<double>(pivot[1]), static_cast<double>(pivot[0]),
          static_cast<double>(lanes[i + 1]), static_cast<double>(lanes[i]),
          static_cast<int>(border));
    }
  }
}

template <class Traits>
static HWY_NOINLINE void TestPartition() {
  using LaneType = typename Traits::LaneType;
  const SortTag<LaneType> d;
  SharedTraits<Traits> st;
  const bool asc = typename Traits::Order().IsAscending();
  const size_t N = Lanes(d);
  constexpr int kDebug = 0;
  constexpr size_t N1 = st.LanesPerKey();
  const size_t base_case_num = SortConstants::BaseCaseNumLanes<N1>(N);
  // left + len + align
  const size_t total = 32 + (base_case_num + 4 * HWY_MAX(N, 4)) + 2 * N;
  auto aligned_lanes = hwy::AllocateAligned<LaneType>(total);
  HWY_ALIGN LaneType buf[SortConstants::BufBytes<LaneType, N1>(HWY_MAX_BYTES) /
                         sizeof(LaneType)];

  for (bool in_asc : {false, true}) {
    for (int left_i : {0, 1, 7, 8, 30, 31}) {
      const size_t left = static_cast<size_t>(left_i) & ~(N1 - 1);
      for (size_t ofs :
           {N, N + 3, 2 * N, 2 * N + 2, 2 * N + 3, 3 * N - 1, 4 * N - 2}) {
        const size_t len = (base_case_num + ofs) & ~(N1 - 1);
        for (LaneType pivot1 : {LaneType(0), LaneType(len / 3),
                                LaneType(2 * len / 3), LaneType(len)}) {
          const LaneType pivot2[2] = {pivot1, 0};
          const auto pivot = st.SetKey(d, pivot2);
          for (size_t misalign = 0; misalign < N;
               misalign += st.LanesPerKey()) {
            LaneType* HWY_RESTRICT lanes = aligned_lanes.get() + misalign;
            const size_t right = left + len;
            if (kDebug) {
              printf(
                  "=========%s asc %d left %d len %d right %d piv %.0f %.0f\n",
                  st.KeyString(), asc, static_cast<int>(left),
                  static_cast<int>(len), static_cast<int>(right),
                  static_cast<double>(pivot2[1]),
                  static_cast<double>(pivot2[0]));
            }

            for (size_t i = 0; i < misalign; ++i) {
              aligned_lanes[i] = hwy::LowestValue<LaneType>();
            }
            for (size_t i = 0; i < left; ++i) {
              lanes[i] = hwy::LowestValue<LaneType>();
            }
            std::unordered_map<LaneType, int> counts;
            for (size_t i = left; i < right; ++i) {
              lanes[i] = static_cast<LaneType>(
                  in_asc ? LaneType(i + 1) - static_cast<LaneType>(left)
                         : static_cast<LaneType>(right) - LaneType(i));
              ++counts[lanes[i]];
              if (kDebug >= 2) {
                printf("%3zu: %f\n", i, static_cast<double>(lanes[i]));
              }
            }
            for (size_t i = right; i < total - misalign; ++i) {
              lanes[i] = hwy::LowestValue<LaneType>();
            }

            size_t border = left + detail::Partition(d, st, lanes + left,
                                                     right - left, pivot, buf);

            if (kDebug >= 2) {
              printf("out>>>>>>\n");
              for (size_t i = left; i < right; ++i) {
                printf("%3zu: %f\n", i, static_cast<double>(lanes[i]));
              }
              for (size_t i = right; i < total - misalign; ++i) {
                printf("%3zu: sentinel %f\n", i, static_cast<double>(lanes[i]));
              }
            }
            for (size_t i = left; i < right; ++i) {
              --counts[lanes[i]];
            }
            for (auto kv : counts) {
              if (kv.second != 0) {
                PrintValue(kv.first);
                HWY_ABORT("Incorrect count %d\n", kv.second);
              }
            }
            VerifyPartition(st, lanes, left, border, right, N1, pivot2);
            for (size_t i = 0; i < misalign; ++i) {
              if (aligned_lanes[i] != hwy::LowestValue<LaneType>())
                HWY_ABORT("Overrun misalign at %d\n", static_cast<int>(i));
            }
            for (size_t i = 0; i < left; ++i) {
              if (lanes[i] != hwy::LowestValue<LaneType>())
                HWY_ABORT("Overrun left at %d\n", static_cast<int>(i));
            }
            for (size_t i = right; i < total - misalign; ++i) {
              if (lanes[i] != hwy::LowestValue<LaneType>())
                HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
            }
          }  // misalign
        }    // pivot
      }      // len
    }        // left
  }          // asc
}

HWY_NOINLINE void TestAllPartition() {
  TestPartition<TraitsLane<OtherOrder<int32_t> > >();
#if !HAVE_INTEL
  TestPartition<Traits128<OrderAscending128> >();
#endif

#if !HWY_IS_DEBUG_BUILD
  TestPartition<TraitsLane<OrderAscending<int16_t> > >();
  TestPartition<TraitsLane<OrderAscending<int64_t> > >();
  TestPartition<TraitsLane<OtherOrder<float> > >();
#if HWY_HAVE_FLOAT64
  TestPartition<TraitsLane<OtherOrder<double> > >();
#endif
#if !HAVE_INTEL
  TestPartition<Traits128<OrderDescending128> >();
#endif
#endif
}

// (used for sample selection for choosing a pivot)
template <typename TU>
static HWY_NOINLINE void TestRandomGenerator() {
  static_assert(!hwy::IsSigned<TU>(), "");
  SortTag<TU> du;
  const size_t N = Lanes(du);

  uint64_t* state = GetGeneratorState();

  // Ensure lower and upper 32 bits are uniformly distributed.
  uint64_t sum_lo = 0, sum_hi = 0;
  for (size_t i = 0; i < 1000; ++i) {
    const uint64_t bits = detail::RandomBits(state);
    sum_lo += bits & 0xFFFFFFFF;
    sum_hi += bits >> 32;
  }
  const double expected = 1000 * (1ULL << 31);
  HWY_ASSERT(0.9 * expected <= static_cast<double>(sum_lo) &&
             static_cast<double>(sum_lo) <= 1.1 * expected);
  HWY_ASSERT(0.9 * expected <= static_cast<double>(sum_hi) &&
             static_cast<double>(sum_hi) <= 1.1 * expected);

  const size_t lanes_per_block = HWY_MAX(64 / sizeof(TU), N);  // power of two

  for (uint32_t num_blocks = 2; num_blocks < 100000;
       num_blocks = 3 * num_blocks / 2) {
    // Generate some numbers and ensure all are in range
    uint64_t sum = 0;
    constexpr size_t kReps = 10000;
    for (size_t rep = 0; rep < kReps; ++rep) {
      const uint32_t bits = detail::RandomBits(state) & 0xFFFFFFFF;
      const size_t index = detail::RandomChunkIndex(num_blocks, bits);
      HWY_ASSERT(((index + 1) * lanes_per_block) <=
                 num_blocks * lanes_per_block);

      sum += index;
    }

    // Also ensure the mean is near the middle of the range
    const double expected = (num_blocks - 1) / 2.0;
    const double actual = static_cast<double>(sum) / kReps;
    HWY_ASSERT(0.9 * expected <= actual && actual <= 1.1 * expected);
  }
}

HWY_NOINLINE void TestAllGenerator() {
  TestRandomGenerator<uint32_t>();
  TestRandomGenerator<uint64_t>();
}

#else
static void TestAllMedian() {}
static void TestAllBaseCase() {}
static void TestAllPartition() {}
static void TestAllGenerator() {}
#endif  // VQSORT_ENABLED

// Remembers input, and compares results to that of a reference algorithm.
template <class Traits>
class CompareResults {
  using LaneType = typename Traits::LaneType;
  using KeyType = typename Traits::KeyType;

 public:
  CompareResults(const LaneType* in, size_t num_lanes) {
    copy_.resize(num_lanes);
    memcpy(copy_.data(), in, num_lanes * sizeof(LaneType));
  }

  bool Verify(const LaneType* output) {
#if HAVE_PDQSORT
    const Algo reference = Algo::kPDQ;
#else
    const Algo reference = Algo::kStd;
#endif
    SharedState shared;
    using Order = typename Traits::Order;
    const Traits st;
    const size_t num_keys = copy_.size() / st.LanesPerKey();
    Run<Order>(reference, reinterpret_cast<KeyType*>(copy_.data()), num_keys,
               shared, /*thread=*/0);
#if VQSORT_PRINT >= 3
    fprintf(stderr, "\nExpected:\n");
    for (size_t i = 0; i < copy_.size(); ++i) {
      PrintValue(copy_[i]);
    }
    fprintf(stderr, "\n");
#endif
    for (size_t i = 0; i < copy_.size(); ++i) {
      if (copy_[i] != output[i]) {
        if (sizeof(KeyType) == 16) {
          fprintf(stderr,
                  "%s Asc %d mismatch at %d of %d: %" PRIu64 " %" PRIu64 "\n",
                  st.KeyString(), Order().IsAscending(), static_cast<int>(i),
                  static_cast<int>(copy_.size()),
                  static_cast<uint64_t>(copy_[i]),
                  static_cast<uint64_t>(output[i]));
        } else {
          fprintf(stderr,
                  "Type %s Asc %d mismatch at %d of %d: ", st.KeyString(),
                  Order().IsAscending(), static_cast<int>(i),
                  static_cast<int>(copy_.size()));
          PrintValue(copy_[i]);
          PrintValue(output[i]);
          fprintf(stderr, "\n");
        }
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<LaneType> copy_;
};

std::vector<Algo> AlgoForTest() {
  return {
#if HAVE_AVX2SORT
    Algo::kSEA,
#endif
#if HAVE_IPS4O
        Algo::kIPS4O,
#endif
#if HAVE_PDQSORT
        Algo::kPDQ,
#endif
#if HAVE_SORT512
        Algo::kSort512,
#endif
        Algo::kHeap, Algo::kVQSort,
  };
}

template <class Traits>
void TestSort(size_t num_lanes) {
// Workaround for stack overflow on clang-cl (/F 8388608 does not help).
#if defined(_MSC_VER)
  return;
#endif
  using Order = typename Traits::Order;
  using LaneType = typename Traits::LaneType;
  using KeyType = typename Traits::KeyType;
  SharedState shared;
  SharedTraits<Traits> st;

  // Round up to a whole number of keys.
  num_lanes += (st.Is128() && (num_lanes & 1));
  const size_t num_keys = num_lanes / st.LanesPerKey();

  constexpr size_t kMaxMisalign = 16;
  auto aligned =
      hwy::AllocateAligned<LaneType>(kMaxMisalign + num_lanes + kMaxMisalign);
  HWY_ASSERT(aligned);
  for (Algo algo : AlgoForTest()) {
    for (Dist dist : AllDist()) {
      for (size_t misalign : {size_t{0}, size_t{st.LanesPerKey()},
                              size_t{3 * st.LanesPerKey()}, kMaxMisalign / 2}) {
        LaneType* lanes = aligned.get() + misalign;

        // Set up red zones before/after the keys to sort
        for (size_t i = 0; i < misalign; ++i) {
          aligned[i] = hwy::LowestValue<LaneType>();
        }
        for (size_t i = 0; i < kMaxMisalign; ++i) {
          lanes[num_lanes + i] = hwy::HighestValue<LaneType>();
        }
#if HWY_IS_MSAN
        __msan_poison(aligned.get(), misalign * sizeof(LaneType));
        __msan_poison(lanes + num_lanes, kMaxMisalign * sizeof(LaneType));
#endif
        InputStats<LaneType> input_stats =
            GenerateInput(dist, lanes, num_lanes);

        CompareResults<Traits> compare(lanes, num_lanes);
        Run<Order>(algo, reinterpret_cast<KeyType*>(lanes), num_keys, shared,
                   /*thread=*/0);
        HWY_ASSERT(compare.Verify(lanes));
        HWY_ASSERT(VerifySort(st, input_stats, lanes, num_lanes, "TestSort"));

        // Check red zones
#if HWY_IS_MSAN
        __msan_unpoison(aligned.get(), misalign * sizeof(LaneType));
        __msan_unpoison(lanes + num_lanes, kMaxMisalign * sizeof(LaneType));
#endif
        for (size_t i = 0; i < misalign; ++i) {
          if (aligned[i] != hwy::LowestValue<LaneType>())
            HWY_ABORT("Overrun left at %d\n", static_cast<int>(i));
        }
        for (size_t i = num_lanes; i < num_lanes + kMaxMisalign; ++i) {
          if (lanes[i] != hwy::HighestValue<LaneType>())
            HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
        }
      }  // misalign
    }    // dist
  }      // algo
}

void TestAllSort() {
  for (int num : {129, 504, 3 * 1000, 34567}) {
    const size_t num_lanes = AdjustedReps(static_cast<size_t>(num));
#if !HAVE_INTEL
    TestSort<TraitsLane<OrderAscending<int16_t> > >(num_lanes);
    TestSort<TraitsLane<OtherOrder<uint16_t> > >(num_lanes);
#endif

    TestSort<TraitsLane<OtherOrder<int32_t> > >(num_lanes);
    TestSort<TraitsLane<OtherOrder<uint32_t> > >(num_lanes);

    TestSort<TraitsLane<OrderAscending<int64_t> > >(num_lanes);
    TestSort<TraitsLane<OrderAscending<uint64_t> > >(num_lanes);

    // WARNING: for float types, SIMD comparisons will flush denormals to
    // zero, causing mismatches with scalar sorts. In this test, we avoid
    // generating denormal inputs.
    TestSort<TraitsLane<OrderAscending<float> > >(num_lanes);
#if HWY_HAVE_FLOAT64  // protects algo-inl's GenerateRandom
    if (HWY_HAVE_FLOAT64) {
      TestSort<TraitsLane<OtherOrder<double> > >(num_lanes);
    }
#endif

// Other algorithms do not support 128-bit keys.
#if !HAVE_VXSORT && !HAVE_INTEL && VQSORT_ENABLED
    TestSort<Traits128<OrderAscending128> >(num_lanes);
    TestSort<Traits128<OrderDescending128> >(num_lanes);

    TestSort<TraitsLane<OrderAscendingKV64> >(num_lanes);
    TestSort<TraitsLane<OrderDescendingKV64> >(num_lanes);

    TestSort<Traits128<OrderAscendingKV128> >(num_lanes);
    TestSort<Traits128<OrderDescendingKV128> >(num_lanes);
#endif
  }
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
namespace {
HWY_BEFORE_TEST(SortTest);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllMedian);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllBaseCase);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllPartition);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllGenerator);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllSort);
}  // namespace
}  // namespace hwy

#endif  // HWY_ONCE
