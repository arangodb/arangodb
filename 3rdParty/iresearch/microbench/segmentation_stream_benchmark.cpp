#include <benchmark/benchmark.h>

#include "analysis/segmentation_token_stream.hpp"

namespace {

using namespace irs::analysis;

void BM_segmentation_analyzer(benchmark::State& state) {
  segmentation_token_stream::options_t opts;
  opts.case_convert = segmentation_token_stream::options_t::case_convert_t::LOWER;
  opts.word_break = segmentation_token_stream::options_t::word_break_t::ALPHA;

  segmentation_token_stream stream(std::move(opts));

  const irs::string_ref str = "QUICK BROWN FOX JUMPS OVER THE LAZY DOG";
  for (auto _ : state) {
    stream.reset(str);
    while (const bool has_next = stream.next()) {
      benchmark::DoNotOptimize(has_next);
    }
  }
}

}

BENCHMARK(BM_segmentation_analyzer);
