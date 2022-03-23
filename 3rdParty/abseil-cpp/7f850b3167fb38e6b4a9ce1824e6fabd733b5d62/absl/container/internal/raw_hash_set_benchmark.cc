// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/container/internal/raw_hash_set.h"

#include <numeric>
#include <random>

#include "absl/base/internal/raw_logging.h"
#include "absl/container/internal/hash_function_defaults.h"
#include "absl/strings/str_format.h"
#include "benchmark/benchmark.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

struct RawHashSetTestOnlyAccess {
  template <typename C>
  static auto GetSlots(const C& c) -> decltype(c.slots_) {
    return c.slots_;
  }
};

namespace {

struct IntPolicy {
  using slot_type = int64_t;
  using key_type = int64_t;
  using init_type = int64_t;

  static void construct(void*, int64_t* slot, int64_t v) { *slot = v; }
  static void destroy(void*, int64_t*) {}
  static void transfer(void*, int64_t* new_slot, int64_t* old_slot) {
    *new_slot = *old_slot;
  }

  static int64_t& element(slot_type* slot) { return *slot; }

  template <class F>
  static auto apply(F&& f, int64_t x) -> decltype(std::forward<F>(f)(x, x)) {
    return std::forward<F>(f)(x, x);
  }
};

class StringPolicy {
  template <class F, class K, class V,
            class = typename std::enable_if<
                std::is_convertible<const K&, absl::string_view>::value>::type>
  decltype(std::declval<F>()(
      std::declval<const absl::string_view&>(), std::piecewise_construct,
      std::declval<std::tuple<K>>(),
      std::declval<V>())) static apply_impl(F&& f,
                                            std::pair<std::tuple<K>, V> p) {
    const absl::string_view& key = std::get<0>(p.first);
    return std::forward<F>(f)(key, std::piecewise_construct, std::move(p.first),
                              std::move(p.second));
  }

 public:
  struct slot_type {
    struct ctor {};

    template <class... Ts>
    slot_type(ctor, Ts&&... ts) : pair(std::forward<Ts>(ts)...) {}

    std::pair<std::string, std::string> pair;
  };

  using key_type = std::string;
  using init_type = std::pair<std::string, std::string>;

  template <class allocator_type, class... Args>
  static void construct(allocator_type* alloc, slot_type* slot, Args... args) {
    std::allocator_traits<allocator_type>::construct(
        *alloc, slot, typename slot_type::ctor(), std::forward<Args>(args)...);
  }

  template <class allocator_type>
  static void destroy(allocator_type* alloc, slot_type* slot) {
    std::allocator_traits<allocator_type>::destroy(*alloc, slot);
  }

  template <class allocator_type>
  static void transfer(allocator_type* alloc, slot_type* new_slot,
                       slot_type* old_slot) {
    construct(alloc, new_slot, std::move(old_slot->pair));
    destroy(alloc, old_slot);
  }

  static std::pair<std::string, std::string>& element(slot_type* slot) {
    return slot->pair;
  }

  template <class F, class... Args>
  static auto apply(F&& f, Args&&... args)
      -> decltype(apply_impl(std::forward<F>(f),
                             PairArgs(std::forward<Args>(args)...))) {
    return apply_impl(std::forward<F>(f),
                      PairArgs(std::forward<Args>(args)...));
  }
};

struct StringHash : container_internal::hash_default_hash<absl::string_view> {
  using is_transparent = void;
};
struct StringEq : std::equal_to<absl::string_view> {
  using is_transparent = void;
};

struct StringTable
    : raw_hash_set<StringPolicy, StringHash, StringEq, std::allocator<int>> {
  using Base = typename StringTable::raw_hash_set;
  StringTable() {}
  using Base::Base;
};

struct IntTable
    : raw_hash_set<IntPolicy, container_internal::hash_default_hash<int64_t>,
                   std::equal_to<int64_t>, std::allocator<int64_t>> {
  using Base = typename IntTable::raw_hash_set;
  IntTable() {}
  using Base::Base;
};

struct string_generator {
  template <class RNG>
  std::string operator()(RNG& rng) const {
    std::string res;
    res.resize(12);
    std::uniform_int_distribution<uint32_t> printable_ascii(0x20, 0x7E);
    std::generate(res.begin(), res.end(), [&] { return printable_ascii(rng); });
    return res;
  }

  size_t size;
};

// Model a cache in steady state.
//
// On a table of size N, keep deleting the LRU entry and add a random one.
void BM_CacheInSteadyState(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  string_generator gen{12};
  StringTable t;
  std::deque<std::string> keys;
  while (t.size() < state.range(0)) {
    auto x = t.emplace(gen(rng), gen(rng));
    if (x.second) keys.push_back(x.first->first);
  }
  ABSL_RAW_CHECK(state.range(0) >= 10, "");
  while (state.KeepRunning()) {
    // Some cache hits.
    std::deque<std::string>::const_iterator it;
    for (int i = 0; i != 90; ++i) {
      if (i % 10 == 0) it = keys.end();
      ::benchmark::DoNotOptimize(t.find(*--it));
    }
    // Some cache misses.
    for (int i = 0; i != 10; ++i) ::benchmark::DoNotOptimize(t.find(gen(rng)));
    ABSL_RAW_CHECK(t.erase(keys.front()), keys.front().c_str());
    keys.pop_front();
    while (true) {
      auto x = t.emplace(gen(rng), gen(rng));
      if (x.second) {
        keys.push_back(x.first->first);
        break;
      }
    }
  }
  state.SetItemsProcessed(state.iterations());
  state.SetLabel(absl::StrFormat("load_factor=%.2f", t.load_factor()));
}

template <typename Benchmark>
void CacheInSteadyStateArgs(Benchmark* bm) {
  // The default.
  const float max_load_factor = 0.875;
  // When the cache is at the steady state, the probe sequence will equal
  // capacity if there is no reclamation of deleted slots. Pick a number large
  // enough to make the benchmark slow for that case.
  const size_t capacity = 1 << 10;

  // Check N data points to cover load factors in [0.4, 0.8).
  const size_t kNumPoints = 10;
  for (size_t i = 0; i != kNumPoints; ++i)
    bm->Arg(std::ceil(
        capacity * (max_load_factor + i * max_load_factor / kNumPoints) / 2));
}
BENCHMARK(BM_CacheInSteadyState)->Apply(CacheInSteadyStateArgs);

void BM_EndComparison(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  string_generator gen{12};
  StringTable t;
  while (t.size() < state.range(0)) {
    t.emplace(gen(rng), gen(rng));
  }

  for (auto _ : state) {
    for (auto it = t.begin(); it != t.end(); ++it) {
      benchmark::DoNotOptimize(it);
      benchmark::DoNotOptimize(t);
      benchmark::DoNotOptimize(it != t.end());
    }
  }
}
BENCHMARK(BM_EndComparison)->Arg(400);

void BM_CopyCtor(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  IntTable t;
  std::uniform_int_distribution<uint64_t> dist(0, ~uint64_t{});

  while (t.size() < state.range(0)) {
    t.emplace(dist(rng));
  }

  for (auto _ : state) {
    IntTable t2 = t;
    benchmark::DoNotOptimize(t2);
  }
}
BENCHMARK(BM_CopyCtor)->Range(128, 4096);

void BM_CopyAssign(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  IntTable t;
  std::uniform_int_distribution<uint64_t> dist(0, ~uint64_t{});
  while (t.size() < state.range(0)) {
    t.emplace(dist(rng));
  }

  IntTable t2;
  for (auto _ : state) {
    t2 = t;
    benchmark::DoNotOptimize(t2);
  }
}
BENCHMARK(BM_CopyAssign)->Range(128, 4096);

void BM_RangeCtor(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<uint64_t> dist(0, ~uint64_t{});
  std::vector<int> values;
  const size_t desired_size = state.range(0);
  while (values.size() < desired_size) {
    values.emplace_back(dist(rng));
  }

  for (auto unused : state) {
    IntTable t{values.begin(), values.end()};
    benchmark::DoNotOptimize(t);
  }
}
BENCHMARK(BM_RangeCtor)->Range(128, 65536);

void BM_NoOpReserveIntTable(benchmark::State& state) {
  IntTable t;
  t.reserve(100000);
  for (auto _ : state) {
    benchmark::DoNotOptimize(t);
    t.reserve(100000);
  }
}
BENCHMARK(BM_NoOpReserveIntTable);

void BM_NoOpReserveStringTable(benchmark::State& state) {
  StringTable t;
  t.reserve(100000);
  for (auto _ : state) {
    benchmark::DoNotOptimize(t);
    t.reserve(100000);
  }
}
BENCHMARK(BM_NoOpReserveStringTable);

void BM_ReserveIntTable(benchmark::State& state) {
  int reserve_size = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    IntTable t;
    state.ResumeTiming();
    benchmark::DoNotOptimize(t);
    t.reserve(reserve_size);
  }
}
BENCHMARK(BM_ReserveIntTable)->Range(128, 4096);

void BM_ReserveStringTable(benchmark::State& state) {
  int reserve_size = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    StringTable t;
    state.ResumeTiming();
    benchmark::DoNotOptimize(t);
    t.reserve(reserve_size);
  }
}
BENCHMARK(BM_ReserveStringTable)->Range(128, 4096);

// Like std::iota, except that ctrl_t doesn't support operator++.
template <typename CtrlIter>
void Iota(CtrlIter begin, CtrlIter end, int value) {
  for (; begin != end; ++begin, ++value) {
    *begin = static_cast<ctrl_t>(value);
  }
}

void BM_Group_Match(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -4);
  Group g{group.data()};
  h2_t h = 1;
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(h);
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.Match(h));
  }
}
BENCHMARK(BM_Group_Match);

void BM_Group_MatchEmpty(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -4);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.MatchEmpty());
  }
}
BENCHMARK(BM_Group_MatchEmpty);

void BM_Group_MatchEmptyOrDeleted(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -4);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.MatchEmptyOrDeleted());
  }
}
BENCHMARK(BM_Group_MatchEmptyOrDeleted);

void BM_Group_CountLeadingEmptyOrDeleted(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -2);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.CountLeadingEmptyOrDeleted());
  }
}
BENCHMARK(BM_Group_CountLeadingEmptyOrDeleted);

void BM_Group_MatchFirstEmptyOrDeleted(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -2);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(*g.MatchEmptyOrDeleted());
  }
}
BENCHMARK(BM_Group_MatchFirstEmptyOrDeleted);

void BM_DropDeletes(benchmark::State& state) {
  constexpr size_t capacity = (1 << 20) - 1;
  std::vector<ctrl_t> ctrl(capacity + 1 + Group::kWidth);
  ctrl[capacity] = ctrl_t::kSentinel;
  std::vector<ctrl_t> pattern = {ctrl_t::kEmpty,   static_cast<ctrl_t>(2),
                                 ctrl_t::kDeleted, static_cast<ctrl_t>(2),
                                 ctrl_t::kEmpty,   static_cast<ctrl_t>(1),
                                 ctrl_t::kDeleted};
  for (size_t i = 0; i != capacity; ++i) {
    ctrl[i] = pattern[i % pattern.size()];
  }
  while (state.KeepRunning()) {
    state.PauseTiming();
    std::vector<ctrl_t> ctrl_copy = ctrl;
    state.ResumeTiming();
    ConvertDeletedToEmptyAndFullToDeleted(ctrl_copy.data(), capacity);
    ::benchmark::DoNotOptimize(ctrl_copy[capacity]);
  }
}
BENCHMARK(BM_DropDeletes);

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl

// These methods are here to make it easy to examine the assembly for targeted
// parts of the API.
auto CodegenAbslRawHashSetInt64Find(absl::container_internal::IntTable* table,
                                    int64_t key) -> decltype(table->find(key)) {
  return table->find(key);
}

bool CodegenAbslRawHashSetInt64FindNeEnd(
    absl::container_internal::IntTable* table, int64_t key) {
  return table->find(key) != table->end();
}

auto CodegenAbslRawHashSetInt64Insert(absl::container_internal::IntTable* table,
                                      int64_t key)
    -> decltype(table->insert(key)) {
  return table->insert(key);
}

bool CodegenAbslRawHashSetInt64Contains(
    absl::container_internal::IntTable* table, int64_t key) {
  return table->contains(key);
}

void CodegenAbslRawHashSetInt64Iterate(
    absl::container_internal::IntTable* table) {
  for (auto x : *table) benchmark::DoNotOptimize(x);
}

int odr =
    (::benchmark::DoNotOptimize(std::make_tuple(
         &CodegenAbslRawHashSetInt64Find, &CodegenAbslRawHashSetInt64FindNeEnd,
         &CodegenAbslRawHashSetInt64Insert,
         &CodegenAbslRawHashSetInt64Contains,
         &CodegenAbslRawHashSetInt64Iterate)),
     1);
