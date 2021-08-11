////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <immer/flex_vector_transient.hpp>

#include "Containers/Enumerate.h"

#include "Replication2/ReplicatedLog/InMemoryLog.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct TestInMemoryLog : InMemoryLog {
  explicit TestInMemoryLog(log_type log) : InMemoryLog(std::move(log)) {}
  TestInMemoryLog() : InMemoryLog(log_type{}) {}
};

struct LogRange {
  LogIndex from;
  LogIndex to;

  LogRange(LogIndex from, LogIndex to) noexcept : from(from), to(to) {
    TRI_ASSERT(from <= to);
  }

  [[nodiscard]] auto empty() const noexcept -> bool { return from == to; }
  [[nodiscard]] auto count() const noexcept -> std::size_t {
    return to.value - from.value;
  }
  [[nodiscard]] auto contains(LogIndex idx) const noexcept -> bool {
    return from <= idx && idx < to;
  }

  friend auto operator<<(std::ostream& os, LogRange const& r) -> std::ostream& {
    return os << "[" << r.from << ", " << r.to << ")";
  }

  friend auto intersect(LogRange a, LogRange b) noexcept -> LogRange {
    auto max_from = std::max(a.from, b.from);
    auto min_to = std::min(a.to, b.to);
    if (max_from > min_to) {
      return {LogIndex{0}, LogIndex{0}};
    } else {
      return {max_from, min_to};
    }
  }

  struct Iterator {
    auto operator++() noexcept -> Iterator& {
      current = current + 1;
      return *this;
    }

    auto operator++(int) noexcept -> Iterator {
      auto idx = current;
      current = current + 1;
      return Iterator(idx);
    }

    auto operator*() const noexcept -> LogIndex { return current; }
    auto operator->() const noexcept -> LogIndex const* { return &current; }
    friend auto operator==(Iterator const& a, Iterator const& b) noexcept -> bool {
      return a.current == b.current;
    }
    friend auto operator!=(Iterator const& a, Iterator const& b) noexcept -> bool {
      return !(a == b);
    }

   private:
    friend LogRange;
    explicit Iterator(LogIndex idx) : current(idx) {}
    LogIndex current;
  };

  [[nodiscard]] auto begin() const -> Iterator { return Iterator{from}; }
  [[nodiscard]] auto end() const -> Iterator { return Iterator{to}; }
};

struct InMemoryLogTestBase {
  static auto createLogForRangeSingleTerm(LogRange range, LogTerm term = LogTerm{1})
      -> TestInMemoryLog {
    auto transient = InMemoryLog::log_type::transient_type{};
    for (auto i : range) {
      transient.push_back(InMemoryLogEntry(
          {term, LogIndex{i}, LogPayload::createFromString("foo")}));
    }
    return TestInMemoryLog(transient.persistent());
  }
};

struct InMemoryLogTest : ::testing::TestWithParam<LogRange>, InMemoryLogTestBase {};

TEST_P(InMemoryLogTest, first_last_next) {
  auto const term = LogTerm{1};
  auto const range = GetParam();
  auto const log = createLogForRangeSingleTerm(range, term);
  auto [from, to] = range;

  EXPECT_EQ(!range.empty(), log.getFirstEntry().has_value());
  EXPECT_EQ(!range.empty(), log.getLastEntry().has_value());
  EXPECT_EQ(log.getNextIndex(), to);

  if (!range.empty()) {
    {
      auto memtry = log.getFirstEntry().value();
      EXPECT_EQ(memtry.entry().logIndex(), from);
    }
    {
      auto memtry = log.getLastEntry().value();
      EXPECT_EQ(memtry.entry().logIndex() + 1, to);
      EXPECT_EQ(log.getLastIndex() + 1, to);
      EXPECT_EQ(log.back().entry().logIndex() + 1, to);

      EXPECT_EQ(memtry.entry().logTerm(), term);
      EXPECT_EQ(log.getLastTerm(), term);
      EXPECT_EQ(log.back().entry().logTerm(), term);
    }
  }
}

TEST_P(InMemoryLogTest, get_entry_by_index) {
  auto const range = GetParam();
  auto const log = createLogForRangeSingleTerm(range);
  auto const tests = {LogIndex{1}, LogIndex{12}, LogIndex{45}};
  for (auto idx : tests) {
    auto memtry = log.getEntryByIndex(idx);
    EXPECT_EQ(range.contains(idx), memtry.has_value())
        << "Range is " << range << " and index is " << idx;
    if (range.contains(idx)) {
      auto entry = memtry->entry();
      EXPECT_EQ(entry.logIndex(), idx);
    }
  }
}

TEST_P(InMemoryLogTest, empty) {
  auto const range = GetParam();
  auto const log = createLogForRangeSingleTerm(range);
  EXPECT_EQ(range.empty(), log.empty());
}

auto const LogRanges = ::testing::Values(LogRange(LogIndex{1}, LogIndex{15}),
                                         LogRange(LogIndex{1}, LogIndex{1234}),
                                         LogRange(LogIndex{1}, LogIndex{1}));

INSTANTIATE_TEST_CASE_P(InMemoryLogTestInstance, InMemoryLogTest, LogRanges);

struct InMemoryLogSliceTest : ::testing::TestWithParam<std::tuple<LogRange, LogRange>>,
                              InMemoryLogTestBase {};

TEST_P(InMemoryLogSliceTest, slice) {
  auto const [range, testRange] = GetParam();
  auto const log = createLogForRangeSingleTerm(range);

  auto s = log.slice(testRange.from, testRange.to);
  auto const expectedRange = intersect(testRange, range);

  ASSERT_EQ(s.size(), expectedRange.count());
  for (auto const& [idx, e] : enumerate(s)) {
    EXPECT_EQ(e.entry().logIndex(), expectedRange.from + idx);
  }
}

TEST_P(InMemoryLogSliceTest, get_iterator_range) {
  auto const [range, testRange] = GetParam();
  auto const log = createLogForRangeSingleTerm(range);

  auto const expectedRange = intersect(range, testRange);
  auto iter = log.getIteratorRange(testRange.from, testRange.to);
  auto [from, to] = iter->range();
  if (expectedRange.empty()) {
    EXPECT_TRUE(from == to);

  } else {
    EXPECT_EQ(from, expectedRange.from);
    EXPECT_EQ(to, expectedRange.to);

    for (auto idx : expectedRange) {
      auto value = iter->next();
      ASSERT_TRUE(value.has_value()) << "idx = " << idx << " range = " << expectedRange;
      EXPECT_EQ(value->logIndex(), idx);
    }
  }

  EXPECT_EQ(iter->next(), std::nullopt);
}

auto const SliceRanges = ::testing::Values(LogRange(LogIndex{4}, LogIndex{6}),
                                           LogRange(LogIndex{1}, LogIndex{8}),
                                           LogRange(LogIndex{100}, LogIndex{120}));

INSTANTIATE_TEST_CASE_P(InMemoryLogSliceTest, InMemoryLogSliceTest,
                        ::testing::Combine(LogRanges, SliceRanges));
