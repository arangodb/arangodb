// Copyright 2019 Google LLC
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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/examples/benchmark.cc"
#include "hwy/foreach_target.h"

#include <stddef.h>
#include <stdio.h>

#include <memory>
#include <numeric>  // iota

#include "hwy/aligned_allocator.h"
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"
HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
#if HWY_TARGET != HWY_SCALAR
using hwy::HWY_NAMESPACE::CombineShiftRightBytes;
#endif

class TwoArray {
 public:
  // Must be a multiple of the vector lane count * 8.
  static size_t NumItems() { return 3456; }

  TwoArray()
      : a_(AllocateAligned<float>(NumItems() * 2)), b_(a_.get() + NumItems()) {
    // = 1, but compiler doesn't know
    const float init = static_cast<float>(Unpredictable1());
    std::iota(a_.get(), a_.get() + NumItems(), init);
    std::iota(b_, b_ + NumItems(), init);
  }

 protected:
  AlignedFreeUniquePtr<float[]> a_;
  float* b_;
};

// Measures durations, verifies results, prints timings.
template <class Benchmark>
void RunBenchmark(const char* caption) {
  printf("%10s: ", caption);
  const size_t kNumInputs = 1;
  const size_t num_items = Benchmark::NumItems() * size_t(Unpredictable1());
  const FuncInput inputs[kNumInputs] = {num_items};
  Result results[kNumInputs];

  Benchmark benchmark;

  Params p;
  p.verbose = false;
  p.max_evals = 7;
  p.target_rel_mad = 0.002;
  const size_t num_results = MeasureClosure(
      [&benchmark](const FuncInput input) { return benchmark(input); }, inputs,
      kNumInputs, results, p);
  if (num_results != kNumInputs) {
    fprintf(stderr, "MeasureClosure failed.\n");
  }

  benchmark.Verify(num_items);

  for (size_t i = 0; i < num_results; ++i) {
    const double cycles_per_item = results[i].ticks / double(results[i].input);
    const double mad = results[i].variability * cycles_per_item;
    printf("%6zu: %6.3f (+/- %5.3f)\n", results[i].input, cycles_per_item, mad);
  }
}

void Intro() {
  HWY_ALIGN const float in[16] = {1, 2, 3, 4, 5, 6};
  HWY_ALIGN float out[16];
  HWY_FULL(float) d;  // largest possible vector
  for (size_t i = 0; i < 16; i += Lanes(d)) {
    const auto vec = Load(d, in + i);  // aligned!
    auto result = vec * vec;
    result += result;  // can update if not const
    Store(result, d, out + i);
  }
  printf("\nF(x)->2*x^2, F(%.0f) = %.1f\n", in[2], out[2]);
}

// BEGINNER: dot product
// 0.4 cyc/float = bronze, 0.25 = silver, 0.15 = gold!
class BenchmarkDot : public TwoArray {
 public:
  BenchmarkDot() : dot_{-1.0f} {}

  FuncOutput operator()(const size_t num_items) {
    HWY_FULL(float) d;
    const size_t N = Lanes(d);
    using V = decltype(Zero(d));
    constexpr size_t unroll = 8;
    // Compiler doesn't make independent sum* accumulators, so unroll manually.
    // Some older compilers might not be able to fit the 8 arrays in registers,
    // so manual unrolling can be helpfull if you run into this issue.
    // 2 FMA ports * 4 cycle latency = 8x unrolled.
    V sum[unroll];
    for (size_t i = 0; i < unroll; ++i) {
      sum[i] = Zero(d);
    }
    const float* const HWY_RESTRICT pa = &a_[0];
    const float* const HWY_RESTRICT pb = b_;
    for (size_t i = 0; i < num_items; i += unroll * N) {
      for (size_t j = 0; j < unroll; ++j) {
        const auto a = Load(d, pa + i + j * N);
        const auto b = Load(d, pb + i + j * N);
        sum[j] = MulAdd(a, b, sum[j]);
      }
    }
    // Reduction tree: sum of all accumulators by pairs into sum[0], then the
    // lanes.
    for (size_t power = 1; power < unroll; power *= 2) {
      for (size_t i = 0; i < unroll; i += 2 * power) {
        sum[i] += sum[i + power];
      }
    }
    dot_ = GetLane(SumOfLanes(d, sum[0]));
    return static_cast<FuncOutput>(dot_);
  }
  void Verify(size_t num_items) {
    if (dot_ == -1.0f) {
      fprintf(stderr, "Dot: must call Verify after benchmark");
      abort();
    }

    const float expected =
        std::inner_product(a_.get(), a_.get() + num_items, b_, 0.0f);
    const float rel_err = std::abs(expected - dot_) / expected;
    if (rel_err > 1.1E-6f) {
      fprintf(stderr, "Dot: expected %e actual %e (%e)\n", expected, dot_,
              rel_err);
      abort();
    }
  }

 private:
  float dot_;  // for Verify
};

// INTERMEDIATE: delta coding
// 1.0 cycles/float = bronze, 0.7 = silver, 0.4 = gold!
struct BenchmarkDelta : public TwoArray {
  FuncOutput operator()(const size_t num_items) const {
#if HWY_TARGET == HWY_SCALAR
    b_[0] = a_[0];
    for (size_t i = 1; i < num_items; ++i) {
      b_[i] = a_[i] - a_[i - 1];
    }
#elif HWY_CAP_GE256
    // Larger vectors are split into 128-bit blocks, easiest to use the
    // unaligned load support to shift between them.
    const HWY_FULL(float) df;
    const size_t N = Lanes(df);
    size_t i;
    b_[0] = a_[0];
    for (i = 1; i < N; ++i) {
      b_[i] = a_[i] - a_[i - 1];
    }
    for (; i < num_items; i += N) {
      const auto a = Load(df, &a_[i]);
      const auto shifted = LoadU(df, &a_[i - 1]);
      Store(a - shifted, df, &b_[i]);
    }
#else  // 128-bit
    // Slightly better than unaligned loads
    const HWY_CAPPED(float, 4) df;
    const size_t N = Lanes(df);
    size_t i;
    b_[0] = a_[0];
    for (i = 1; i < N; ++i) {
      b_[i] = a_[i] - a_[i - 1];
    }
    auto prev = Load(df, &a_[0]);
    for (; i < num_items; i += Lanes(df)) {
      const auto a = Load(df, &a_[i]);
      const auto shifted = CombineShiftRightLanes<3>(a, prev);
      prev = a;
      Store(a - shifted, df, &b_[i]);
    }
#endif
    return static_cast<FuncOutput>(b_[num_items - 1]);
  }

  void Verify(size_t num_items) {
    for (size_t i = 0; i < num_items; ++i) {
      const float expected = (i == 0) ? a_[0] : a_[i] - a_[i - 1];
      const float err = std::abs(expected - b_[i]);
      if (err > 1E-6f) {
        fprintf(stderr, "Delta: expected %e, actual %e\n", expected, b_[i]);
      }
    }
  }
};

void RunBenchmarks() {
  Intro();
  printf("------------------------ %s\n", TargetName(HWY_TARGET));
  RunBenchmark<BenchmarkDot>("dot");
  RunBenchmark<BenchmarkDelta>("delta");
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
HWY_EXPORT(RunBenchmarks);

void Run() {
  for (uint32_t target : SupportedAndGeneratedTargets()) {
    SetSupportedTargetsForTest(target);
    HWY_DYNAMIC_DISPATCH(RunBenchmarks)();
  }
  SetSupportedTargetsForTest(0);  // Reset the mask afterwards.
}

}  // namespace hwy

int main(int /*argc*/, char** /*argv*/) {
  hwy::Run();
  return 0;
}
#endif  // HWY_ONCE
