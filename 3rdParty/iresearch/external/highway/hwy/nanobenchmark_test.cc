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

#include "hwy/nanobenchmark.h"

#include <stdio.h>
#include <stdlib.h>  // strtol
#include <unistd.h>  // sleep

#include <random>

namespace hwy {
namespace {

FuncOutput Div(const void*, FuncInput in) {
  // Here we're measuring the throughput because benchmark invocations are
  // independent. Any dividend will do; the divisor is nonzero.
  return 0xFFFFF / in;
}

template <size_t N>
void MeasureDiv(const FuncInput (&inputs)[N]) {
  Result results[N];
  Params params;
  params.max_evals = 4;  // avoid test timeout
  const size_t num_results = Measure(&Div, nullptr, inputs, N, results, params);
  for (size_t i = 0; i < num_results; ++i) {
    printf("%5zu: %6.2f ticks; MAD=%4.2f%%\n", results[i].input,
           results[i].ticks, results[i].variability * 100.0);
  }
}

std::mt19937 rng;

// A function whose runtime depends on rng.
FuncOutput Random(const void* /*arg*/, FuncInput in) {
  const size_t r = rng() & 0xF;
  uint32_t ret = in;
  for (size_t i = 0; i < r; ++i) {
    ret /= ((rng() & 1) + 2);
  }
  return ret;
}

// Ensure the measured variability is high.
template <size_t N>
void MeasureRandom(const FuncInput (&inputs)[N]) {
  Result results[N];
  Params p;
  p.max_evals = 4;  // avoid test timeout
  p.verbose = false;
  const size_t num_results = Measure(&Random, nullptr, inputs, N, results, p);
  for (size_t i = 0; i < num_results; ++i) {
    NANOBENCHMARK_CHECK(results[i].variability > 1E-3);
  }
}

template <size_t N>
void EnsureLongMeasurementFails(const FuncInput (&inputs)[N]) {
  printf("Expect a 'measurement failed' below:\n");
  Result results[N];

  const size_t num_results = Measure(
      [](const void*, const FuncInput input) -> FuncOutput {
        // Loop until the sleep succeeds (not interrupted by signal). We assume
        // >= 512 MHz, so 2 seconds will exceed the 1 << 30 tick safety limit.
        while (sleep(2) != 0) {
        }
        return input;
      },
      nullptr, inputs, N, results);
  NANOBENCHMARK_CHECK(num_results == 0);
  (void)num_results;
}

void RunAll(const int argc, char** /*argv*/) {
  // unpredictable == 1 but the compiler doesn't know that.
  const int unpredictable = argc != 999;
  static const FuncInput inputs[] = {static_cast<FuncInput>(unpredictable) + 2,
                                     static_cast<FuncInput>(unpredictable + 9)};

  MeasureDiv(inputs);
  MeasureRandom(inputs);
  EnsureLongMeasurementFails(inputs);
}

}  // namespace
}  // namespace hwy

int main(int argc, char* argv[]) {
  hwy::RunAll(argc, argv);
  return 0;
}
