////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#include <benchmark/benchmark.h>

#include "analysis/segmentation_token_stream.hpp"

namespace {

using namespace irs::analysis;

void BM_segmentation_analyzer(benchmark::State& state) {
  segmentation_token_stream::options_t opts;
  opts.case_convert =
    segmentation_token_stream::options_t::case_convert_t::LOWER;
  opts.word_break = segmentation_token_stream::options_t::word_break_t::ALPHA;

  segmentation_token_stream stream(std::move(opts));

  const std::string_view str = "QUICK BROWN FOX JUMPS OVER THE LAZY DOG";
  for (auto _ : state) {
    stream.reset(str);
    while (bool has_next = stream.next()) {
      benchmark::DoNotOptimize(has_next);
    }
  }
}

}  // namespace

BENCHMARK(BM_segmentation_analyzer);
