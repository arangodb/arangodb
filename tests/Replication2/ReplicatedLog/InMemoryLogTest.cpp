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
  explicit TestInMemoryLog(log_type log, LogIndex first)
      : InMemoryLog(std::move(log), first) {}
  TestInMemoryLog() : InMemoryLog(log_type{}) {}
};

struct InMemoryLogTestBase {
  static auto createLogForRangeSingleTerm(LogRange range,
                                          LogTerm term = LogTerm{1})
      -> TestInMemoryLog {
    auto transient = InMemoryLog::log_type::transient_type{};
    for (auto i : range) {
      transient.push_back(InMemoryLogEntry(
          {term, LogIndex{i}, LogPayload::createFromString("foo")}));
    }
    return TestInMemoryLog(transient.persistent(), range.from);
  }
};

struct InMemoryLogTest : ::testing::TestWithParam<LogRange>,
                         InMemoryLogTestBase {};

TEST_P(InMemoryLogTest, first_last_next) {
  auto const term = LogTerm{1};
  auto const range = GetParam();
  auto const log = createLogForRangeSingleTerm(range, term);
  auto [from, to] = range;

  EXPECT_EQ(!range.empty(), log.getFirstEntry().has_value());
  EXPECT_EQ(!range.empty(), log.getLastEntry().has_value());
  EXPECT_EQ(log.getNextIndex(), to);

  EXPECT_EQ(log.getIndexRange(), range);

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

TEST_P(InMemoryLogTest, append_in_place) {
  auto const range = GetParam();
  auto log = createLogForRangeSingleTerm(range);

  auto memtry = InMemoryLogEntry(
      {LogTerm{1}, range.to, LogPayload::createFromString("foo")});
  log.appendInPlace(LoggerContext(Logger::FIXME), std::move(memtry));
  {
    auto result = log.getEntryByIndex(range.to);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->entry().logIndex(), range.to);
  }
  {
    auto result = log.getLastEntry();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->entry().logIndex(), range.to);
  }
}

auto const LogRanges = ::testing::Values(
    LogRange(LogIndex{1}, LogIndex{15}), LogRange(LogIndex{1}, LogIndex{1234}),
    LogRange(LogIndex{1}, LogIndex{1}), LogRange(LogIndex{5}, LogIndex{18}),
    LogRange(LogIndex{76}, LogIndex{76}));

INSTANTIATE_TEST_CASE_P(InMemoryLogTestInstance, InMemoryLogTest, LogRanges);

struct InMemoryLogAppendTest
    : testing::TestWithParam<std::tuple<std::size_t, LogRange>>,
      InMemoryLogTestBase {
  static auto getPersistedEntriesVector(LogIndex first, std::size_t length,
                                        LogTerm term = LogTerm{1}) {
    auto result = InMemoryLog::log_type_persisted::transient_type{};
    for (auto idx : LogRange(first, first + length)) {
      result.push_back(
          PersistingLogEntry{term, idx, LogPayload::createFromString("foo")});
    }
    return result.persistent();
  }
};

TEST_P(InMemoryLogAppendTest, append_peristed_entries) {
  auto const [length, range] = GetParam();
  auto const log = createLogForRangeSingleTerm(range, LogTerm{1});
  auto const toAppend = getPersistedEntriesVector(range.to, length, LogTerm{2});

  auto const newLog = log.append(LoggerContext(Logger::FIXME), toAppend);
  for (auto idx : LogRange(range.from, range.to + length)) {
    auto memtry = newLog.getEntryByIndex(idx);
    ASSERT_TRUE(memtry.has_value()) << "idx = " << idx;
    auto const expectedTerm = range.contains(idx) ? LogTerm{1} : LogTerm{2};
    EXPECT_EQ(memtry->entry().logIndex(), idx);
    EXPECT_EQ(memtry->entry().logTerm(), expectedTerm);
  }
}

INSTANTIATE_TEST_CASE_P(InMemoryLogAppendTest, InMemoryLogAppendTest,
                        ::testing::Combine(::testing::Range<std::size_t>(0, 10),
                                           LogRanges));

struct InMemoryLogSliceTest
    : ::testing::TestWithParam<std::tuple<LogRange, LogRange>>,
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
      ASSERT_TRUE(value.has_value())
          << "idx = " << idx << " range = " << expectedRange;
      EXPECT_EQ(value->logIndex(), idx);
    }
  }

  EXPECT_EQ(iter->next(), std::nullopt);
}

TEST_P(InMemoryLogSliceTest, get_iterator_from) {
  auto [range, testRange] = GetParam();
  auto const log = createLogForRangeSingleTerm(range);
  testRange.to = range.to;  // no bound on to

  auto const expectedRange = intersect(range, testRange);
  auto iter = log.getIteratorFrom(testRange.from);

  for (auto idx : expectedRange) {
    auto value = iter->next();
    ASSERT_TRUE(value.has_value())
        << "idx = " << idx << " range = " << expectedRange;
    EXPECT_EQ(value->logIndex(), idx);
  }

  EXPECT_EQ(iter->next(), std::nullopt);
}

TEST_P(InMemoryLogSliceTest, release) {
  auto [range, testRange] = GetParam();
  auto const log = createLogForRangeSingleTerm(range);
  testRange.to = range.to;
  auto const expectedRange = intersect(range, testRange);
  if (!expectedRange.empty()) {
    auto newLog = log.release(testRange.from);
    EXPECT_EQ(newLog.getIndexRange(), expectedRange);
  }
}

auto const SliceRanges = ::testing::Values(
    LogRange(LogIndex{4}, LogIndex{6}), LogRange(LogIndex{1}, LogIndex{8}),
    LogRange(LogIndex{100}, LogIndex{120}),
    LogRange(LogIndex{18}, LogIndex{18}));

INSTANTIATE_TEST_CASE_P(InMemoryLogSliceTest, InMemoryLogSliceTest,
                        ::testing::Combine(LogRanges, SliceRanges));

using TermDistribution = std::map<LogTerm, std::size_t>;

using TermTestData = std::tuple<LogTerm, LogIndex, TermDistribution>;

struct IndexOfTermTest : ::testing::TestWithParam<TermTestData>,
                         InMemoryLogTestBase {
  static auto createLogForDistribution(LogIndex first,
                                       TermDistribution const& dist)
      -> TestInMemoryLog {
    auto transient = InMemoryLog::log_type::transient_type{};
    auto next = first;
    for (auto [term, length] : dist) {
      for (auto idx : LogRange(next, next + length)) {
        transient.push_back(
            InMemoryLogEntry({term, idx, LogPayload::createFromString("foo")}));
      }
      next = next + length;
    }
    return TestInMemoryLog(transient.persistent());
  }

  static auto getTermBounds(LogIndex first, TermDistribution const& dist,
                            LogTerm wanted) -> std::optional<LogRange> {
    auto next = first;
    for (auto [term, length] : dist) {
      if (term == wanted) {
        return LogRange{next, next + length};
      }
      next = next + length;
    }

    return std::nullopt;
  }
};

TEST_P(IndexOfTermTest, first_index_of_term) {
  auto [term, first, dist] = GetParam();
  auto log = createLogForDistribution(first, dist);

  auto range = getTermBounds(first, dist, term);
  auto firstInTerm = log.getFirstIndexOfTerm(term);
  auto lastInTerm = log.getLastIndexOfTerm(term);

  ASSERT_EQ(range.has_value(), firstInTerm.has_value());
  ASSERT_EQ(range.has_value(), lastInTerm.has_value());

  if (range.has_value()) {
    EXPECT_EQ(range->from, *firstInTerm)
        << "term = " << term << " log = " << log.dump();
    EXPECT_EQ(range->to, *lastInTerm + 1);
  }
}

auto Distributions = ::testing::Values(
    TermDistribution{
        {LogTerm{1}, 5},
    },
    TermDistribution{
        {LogTerm{1}, 5},
        {LogTerm{2}, 18},
    },
    TermDistribution{
        {LogTerm{1}, 5},
        {LogTerm{2}, 18},
    },
    TermDistribution{
        {LogTerm{1}, 5},
        {LogTerm{2}, 18},
        {LogTerm{3}, 18},
    },
    TermDistribution{
        {LogTerm{1}, 5},
        {LogTerm{2}, 18},
        {LogTerm{3}, 18},
    });

INSTANTIATE_TEST_CASE_P(
    IndexOfTermTest, IndexOfTermTest,
    ::testing::Combine(::testing::Values(LogTerm{1}, LogTerm{2}, LogTerm{3}),
                       ::testing::Values(LogIndex{1}, LogIndex{10}),
                       Distributions));
