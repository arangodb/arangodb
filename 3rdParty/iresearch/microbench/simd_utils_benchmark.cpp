#include <benchmark/benchmark.h>

extern "C" {
#include <simdcomputil.h>
}

#include "store/store_utils.hpp"
#include "utils/simd_utils.hpp"
#include "utils/misc.hpp"

namespace {

constexpr size_t BLOCK_SIZE = 65536;

struct HWY_ALIGN aligned_type { };

// -----------------------------------------------------------------------------
// --SECTION--                                                             delta
// -----------------------------------------------------------------------------

void BM_delta_encode(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  for (auto _ : state) {
    std::iota(std::begin(values), std::end(values), 0);
    irs::encode::delta::encode(std::begin(values), std::end(values));
  }
}

BENCHMARK(BM_delta_encode);

void BM_delta_encode_simd_unaligned(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  for (auto _ : state) {
    std::iota(std::begin(values), std::end(values), 0);
    irs::simd::delta_encode<IRESEARCH_COUNTOF(values), false>(values, 0U);
  }
}

BENCHMARK(BM_delta_encode_simd_unaligned);

// -----------------------------------------------------------------------------
// --SECTION--                                                      avg_encode32
// -----------------------------------------------------------------------------

void BM_avg_encode32(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  for (auto _ : state) {
    std::iota(std::begin(values), std::end(values), ::rand());
    irs::encode::avg::encode(std::begin(values), std::end(values));
  }
}

BENCHMARK(BM_avg_encode32);

void BM_avg_encode32_simd_aligned(benchmark::State& state) {
  HWY_ALIGN uint32_t values[BLOCK_SIZE];
  for (auto _ : state) {
    std::iota(std::begin(values), std::end(values), ::rand());
    irs::simd::avg_encode<IRESEARCH_COUNTOF(values), true>(values);
  }
}

BENCHMARK(BM_avg_encode32_simd_aligned);

void BM_avg_encode32_simd_unaligned(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  for (auto _ : state) {
    std::iota(std::begin(values), std::end(values), ::rand());
    irs::simd::avg_encode<IRESEARCH_COUNTOF(values), false>(values);
  }
}

BENCHMARK(BM_avg_encode32_simd_unaligned);

// -----------------------------------------------------------------------------
// --SECTION--                                                      avg_encode64
// -----------------------------------------------------------------------------

void BM_avg_encode64(benchmark::State& state) {
  uint64_t values[BLOCK_SIZE];
  for (auto _ : state) {
    std::iota(std::begin(values), std::end(values), ::rand());
    auto stats = irs::encode::avg::encode(std::begin(values), std::end(values));
    benchmark::DoNotOptimize(stats);
  }
}

BENCHMARK(BM_avg_encode64);

// -----------------------------------------------------------------------------
// --SECTION--                                                          maxmin64
// -----------------------------------------------------------------------------

void BM_maxmin64(benchmark::State& state) {
  uint64_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto min = values[0];
    auto max = min;

    for (auto* begin = values + 1; begin != std::end(values); ++begin) {
      min = std::min(min, *begin);
      max = std::max(max, *begin);
    }

    benchmark::DoNotOptimize(min);
    benchmark::DoNotOptimize(max);
  }
}

BENCHMARK(BM_maxmin64);

void BM_maxmin64_simd_aligned(benchmark::State& state) {
  HWY_ALIGN uint64_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto maxmin = irs::simd::maxmin<IRESEARCH_COUNTOF(values), true>(values);
    benchmark::DoNotOptimize(maxmin);
  }
}

BENCHMARK(BM_maxmin64_simd_aligned);

void BM_maxmin64_simd_unaligned(benchmark::State& state) {
  uint64_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto maxmin = irs::simd::maxmin<IRESEARCH_COUNTOF(values), false>(values);
    benchmark::DoNotOptimize(maxmin);
  }
}

BENCHMARK(BM_maxmin64_simd_unaligned);

// -----------------------------------------------------------------------------
// --SECTION--                                                          maxmin32
// -----------------------------------------------------------------------------

void BM_maxmin32(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto min = values[0];
    auto max = min;

    for (auto* begin = values + 1; begin != std::end(values); ++begin) {
      min = std::min(min, *begin);
      max = std::max(max, *begin);
    }

    benchmark::DoNotOptimize(min);
    benchmark::DoNotOptimize(max);
  }
}

BENCHMARK(BM_maxmin32);

void BM_maxmin32_simd_aligned(benchmark::State& state) {
  HWY_ALIGN uint32_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto maxmin = irs::simd::maxmin<IRESEARCH_COUNTOF(values), true>(values);
    benchmark::DoNotOptimize(maxmin);
  }
}

BENCHMARK(BM_maxmin32_simd_aligned);

void BM_maxmin32_simd_unaligned(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto maxmin = irs::simd::maxmin<IRESEARCH_COUNTOF(values), false>(values);
    benchmark::DoNotOptimize(maxmin);
  }
}

BENCHMARK(BM_maxmin32_simd_unaligned);

// -----------------------------------------------------------------------------
// --SECTION--                                                         maxbits64
// -----------------------------------------------------------------------------

void BM_maxbits64(benchmark::State& state) {
  uint64_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto bits = irs::packed::maxbits64(std::begin(values), std::end(values));
    benchmark::DoNotOptimize(bits);
  }
}

BENCHMARK(BM_maxbits64);

void BM_maxbits64_simd_aligned(benchmark::State& state) {
  HWY_ALIGN uint64_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto bits = irs::simd::maxbits<IRESEARCH_COUNTOF(values), true>(values);
    benchmark::DoNotOptimize(bits);
  }
}

BENCHMARK(BM_maxbits64_simd_aligned);

void BM_maxbits64_simd_unaligned(benchmark::State& state) {
  uint64_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());
  for (auto _ : state) {
    auto maxbits = irs::simd::maxbits<IRESEARCH_COUNTOF(values), false>(values);
    benchmark::DoNotOptimize(maxbits);
  }
}

BENCHMARK(BM_maxbits64_simd_unaligned);

// -----------------------------------------------------------------------------
// --SECTION--                                                         maxbits32
// -----------------------------------------------------------------------------

void BM_maxbits32(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());

  for (auto _ : state) {
    auto bits = irs::packed::maxbits32(std::begin(values), std::end(values));
    benchmark::DoNotOptimize(bits);
  }
}

BENCHMARK(BM_maxbits32);

void BM_maxbits32_lemire(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());

  for (auto _ : state) {
    uint32_t bits = 0;
    for (auto begin = values; begin != std::end(values); begin += SIMDBlockSize) {
      bits = std::max(maxbits(begin), bits);
    }
    benchmark::DoNotOptimize(bits);
  }
}

BENCHMARK(BM_maxbits32_lemire);

void BM_maxbits32_simd_aligned(benchmark::State& state) {
  HWY_ALIGN uint32_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());

  for (auto _ : state) {
    auto maxbits = irs::simd::maxbits<IRESEARCH_COUNTOF(values), false>(values);
    benchmark::DoNotOptimize(maxbits);
  }
}

BENCHMARK(BM_maxbits32_simd_aligned);

void BM_maxbits32_simd_unaligned(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  std::iota(std::begin(values), std::end(values), ::rand());

  for (auto _ : state) {
    auto maxbits = irs::simd::maxbits<IRESEARCH_COUNTOF(values), false>(values);
    benchmark::DoNotOptimize(maxbits);
  }
}

BENCHMARK(BM_maxbits32_simd_unaligned);

// -----------------------------------------------------------------------------
// --SECTION--                                                       all_equal32
// -----------------------------------------------------------------------------

void BM_all_equal32(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::irstd::all_equal(std::begin(values), std::end(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal32);

void BM_all_equal32_simd_unaligned(benchmark::State& state) {
  uint32_t values[BLOCK_SIZE];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::simd::all_equal<false>(std::begin(values), IRESEARCH_COUNTOF(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal32_simd_unaligned);

void BM_all_equal32_small(benchmark::State& state) {
  uint32_t values[32];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::irstd::all_equal(std::begin(values), std::end(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal32_small);

void BM_all_equal32_small_simd_unaligned(benchmark::State& state) {
  uint32_t values[32];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::simd::all_equal<false>(std::begin(values), IRESEARCH_COUNTOF(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal32_small_simd_unaligned);

// -----------------------------------------------------------------------------
// --SECTION--                                                       all_equal64
// -----------------------------------------------------------------------------

void BM_all_equal64(benchmark::State& state) {
  uint64_t values[BLOCK_SIZE];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::irstd::all_equal(std::begin(values), std::end(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal64);

void BM_all_equal64_simd_unaligned(benchmark::State& state) {
  uint64_t values[BLOCK_SIZE];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::simd::all_equal<false>(std::begin(values), IRESEARCH_COUNTOF(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal64_simd_unaligned);

void BM_all_equal64_small(benchmark::State& state) {
  uint64_t values[64];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::irstd::all_equal(std::begin(values), std::end(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal64_small);

void BM_all_equal64_small_simd_unaligned(benchmark::State& state) {
  uint64_t values[64];
  std::fill_n(std::begin(values), IRESEARCH_COUNTOF(values), ::rand());

  for (auto _ : state) {
    auto res = irs::simd::all_equal<false>(std::begin(values), IRESEARCH_COUNTOF(values));
    benchmark::DoNotOptimize(res);
  }
}

BENCHMARK(BM_all_equal64_small_simd_unaligned);

}
