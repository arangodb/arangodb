#include <benchmark/benchmark.h>
#include "frozen/algorithm.h"
#include <algorithm>
#include <functional>
#include <cstring>

static char const Words [] = R"(
Let it go, let it go
Can't hold it back anymore
Let it go, let it go
Turn my back and slam the door
The snow blows white on the mountain tonight
Not a footprint to be seen
A kingdom of isolation and it looks like I'm the queen
The wind is howling like the swirling storm inside
Couldn't keep it in
Heaven knows I try
Don't let them in, don't let them see
Be the good girl you always had to be
Conceal, don't feel, don't let them know
Well now they know
Let it go, let it go
Can't hold you back anymore
Let it go, let it go
Turn my back and slam the door
And here I stand
And here I'll stay
Let it go, let it go
The cold never bothered me anyway
It's funny how some distance makes everything seem small
And the fears that once controlled me can't get to me at all
Up here
)";

static auto * volatile WordsPtr = &Words;

static constexpr  char Word[] = "controlled";

static void BM_StrFzSearchInBM(benchmark::State& state) {
  for (auto _ : state) {
    volatile bool status = frozen::search(std::begin(*WordsPtr), std::end(*WordsPtr), frozen::make_boyer_moore_searcher(Word));
  }
}
BENCHMARK(BM_StrFzSearchInBM);

static void BM_StrStdSearchInBM(benchmark::State& state) {
  for (auto _ : state) {
    volatile bool status = std::search(std::begin(*WordsPtr), std::end(*WordsPtr),
                                       std::boyer_moore_searcher<char const*>(std::begin(Word), std::end(Word)));
  }
}
BENCHMARK(BM_StrStdSearchInBM);

static void BM_StrFzSearchInKMP(benchmark::State& state) {
  for (auto _ : state) {
    volatile bool status = frozen::search(std::begin(*WordsPtr), std::end(*WordsPtr), frozen::make_knuth_morris_pratt_searcher(Word));
  }
}
BENCHMARK(BM_StrFzSearchInKMP);

#if 0
static void BM_StrStdSearchInStrStr(benchmark::State& state) {
  for (auto _ : state) {
    char const* volatile status = std::strstr(*WordsPtr, Word);
  }
}
BENCHMARK(BM_StrStdSearchInStrStr);
#endif
