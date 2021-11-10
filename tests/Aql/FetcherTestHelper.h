////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef TESTS_AQL_FETCHER_TESTHELPER_H
#define TESTS_AQL_FETCHER_TESTHELPER_H

#include "AqlItemBlockHelper.h"
#include "DependencyProxyMock.h"
#include "gtest/gtest.h"

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

#include <Containers/HashSet.h>

namespace arangodb {
namespace tests {
namespace aql {

/// SECTION: Shadow row combination tests. Split the input block into diverse
/// parts to securely test all borders

namespace fetcherHelper {

template <class Fetcher>
class PatternTestWrapper {
 protected:
  PatternTestWrapper() {}

 public:
  virtual ~PatternTestWrapper() = default;

  Fetcher& getFetcher() { return _fetcher; }

  AqlItemBlockManager& itemBlockManager() { return _itemBlockManager; }

  void shouldReturn(std::vector<std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr>>& toReturn) {
    _dependencyProxyMock.shouldReturn(toReturn);
  }

  /// helper method to assert that next up we will produce exactly the given set
  /// of DataRows For simplicity we only test string values here, that AqlValues
  /// of different types work is tested somewhere else.
  virtual void PullAndAssertDataRows(std::vector<std::string> const& dataResults) = 0;

  /// Helper method to assert that we can now pull all ShadowRows at once, and stop at the next data row
  /// Also asserts that we will never leave the finalState (either HASMORE or DONE)
  void PullAndAssertShadowRows(std::vector<std::pair<uint64_t, std::string>> shadowResults,
                               ExecutionState finalState) {
    ExecutionState state = ExecutionState::HASMORE;
    ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};
    // For each entry in shadowResults.
    // We can fetch 1 shadow row matching it.
    for (auto const& it : shadowResults) {
      std::tie(state, shadow) = _fetcher.fetchShadowRow();
      if (it == shadowResults.back()) {
        // The last element will reach the final state
        EXPECT_EQ(state, finalState);
      } else {
        EXPECT_EQ(state, ExecutionState::HASMORE);
      }
      // We are awaiting a shadow row now
      ASSERT_TRUE(shadow.isInitialized());
      // Assert the data
      EXPECT_TRUE(shadow.getValue(0).slice().isEqualString(it.second));
      EXPECT_EQ(shadow.getDepth(), it.first);
    }
    // Will stay on FinalState
    std::tie(state, shadow) = _fetcher.fetchShadowRow();
    EXPECT_EQ(state, finalState);
    EXPECT_FALSE(shadow.isInitialized());
  }

  void StaysConstantAfterDone() {
    PullAndAssertDataRows({});
    PullAndAssertShadowRows({}, ExecutionState::DONE);
  }

 private:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  RegIdSet inputRegisters;
  DependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> _dependencyProxyMock{_monitor, inputRegisters, 1};
  AqlItemBlockManager _itemBlockManager{&_monitor, SerializationFormat::SHADOWROWS};

 protected:
  Fetcher _fetcher{_dependencyProxyMock};
};

/// Helper method that splits a single baseBlock into multiple AqlItemBlocks.
/// Where to split is defined by the piecesBitMap handed it.
/// If the `n-th` bit piecesBitMap is set, we will add a split after Row `n`.
/// e.g. we will now have a block from 0 -> n and a block from n+1 -> end
/// we can apply multiple of these splits, ulimate case, split block into single line blocks.
static std::vector<std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr>> CutMyBlockIntoPieces(
    SharedAqlItemBlockPtr baseBlock, uint64_t piecesBitMap) {
  std::vector<std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr>> toReturn{};
  size_t from = 0;
  for (size_t to = 0; to < baseBlock->size(); ++to) {
    if (((piecesBitMap) >> (to)) & 1) {
      // We split blocks if the corresponding bit is set.
      ExecutionState state = to == baseBlock->size() - 1 ? ExecutionState::DONE
                                                         : ExecutionState::HASMORE;
      toReturn.emplace_back(std::make_pair(state, baseBlock->slice(from, to + 1)));
      from = to + 1;
    }
  }
  if (from < baseBlock->size()) {
    toReturn.emplace_back(std::make_pair(ExecutionState::DONE,
                                         baseBlock->slice(from, baseBlock->size())));
  }
  return toReturn;
}

// Section First Pattern, alternating Input / Shadow Rows, 1 higher level shadow row
#define TEST_SHADOWROW_PATTERN_1(FetcherWrapper, TestName)                                                    \
  class TestName : public testing::TestWithParam<uint64_t> {                                                  \
   protected:                                                                                                 \
    FetcherWrapper wrapper;                                                                                   \
                                                                                                              \
    void SetUp() override {                                                                                   \
      SharedAqlItemBlockPtr baseBlock = buildBlock<1>(                                                        \
          wrapper.itemBlockManager(),                                                                         \
          {{{R"("a")"}}, {{R"("a")"}}, {{R"("b")"}}, {{R"("b")"}}, {{R"("c")"}}, {{R"("c")"}}, {{R"("c")"}}}, \
          {{1, 0}, {3, 0}, {5, 0}, {6, 1}});                                                                  \
      /* prepare the proxy */                                                                                 \
      uint64_t splits = GetParam();                                                                           \
      ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));                                                \
      auto toReturn = fetcherHelper::CutMyBlockIntoPieces(baseBlock, splits);                                 \
      wrapper.shouldReturn(toReturn);                                                                         \
    }                                                                                                         \
  };                                                                                                          \
                                                                                                              \
  TEST_P(TestName, handle_shadow_rows) {                                                                      \
    /* Fetch the input data */                                                                                \
    wrapper.PullAndAssertDataRows({"a"});                                                                     \
    /* Fetch the shadow row */                                                                                \
    wrapper.PullAndAssertShadowRows({{0, "a"}}, ExecutionState::HASMORE);                                     \
    /* Fetch the input data */                                                                                \
    wrapper.PullAndAssertDataRows({"b"});                                                                     \
    /* Fetch the shadow row */                                                                                \
    wrapper.PullAndAssertShadowRows({{0, "b"}}, ExecutionState::HASMORE);                                     \
    /* Fetch the input data */                                                                                \
    wrapper.PullAndAssertDataRows({"c"});                                                                     \
    /* Fetch the shadow row and the higher level shadow row */                                                \
    wrapper.PullAndAssertShadowRows({{0, "c"}, {1, "c"}}, ExecutionState::DONE);                              \
    /* Done check */                                                                                          \
    wrapper.StaysConstantAfterDone();                                                                         \
  }                                                                                                           \
                                                                                                              \
  INSTANTIATE_TEST_CASE_P(TestName##Instanciated, TestName,                                                   \
                          testing::Range(static_cast<uint64_t>(0),                                            \
                                         static_cast<uint64_t>(std::pow(2, 6))));

// Section Second Pattern, two consecutive relevant shadow rows, , 1 higher level shadow row
#define TEST_SHADOWROW_PATTERN_2(FetcherWrapper, TestName)                        \
  class TestName : public testing::TestWithParam<uint64_t> {                      \
   protected:                                                                     \
    FetcherWrapper wrapper;                                                       \
                                                                                  \
    void SetUp() override {                                                       \
      SharedAqlItemBlockPtr baseBlock =                                           \
          buildBlock<1>(wrapper.itemBlockManager(),                               \
                        {{{R"("a")"}}, {{R"("a")"}}, {{R"("b")"}}, {{R"("b")"}}}, \
                        {{1, 0}, {2, 0}, {3, 1}});                                \
      /* prepare the proxy */                                                     \
      uint64_t splits = GetParam();                                               \
      ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));                    \
      auto toReturn = fetcherHelper::CutMyBlockIntoPieces(baseBlock, splits);     \
      wrapper.shouldReturn(toReturn);                                             \
    }                                                                             \
  };                                                                              \
                                                                                  \
  TEST_P(TestName, handle_shadow_rows) {                                          \
    /* Fetch the input data */                                                    \
    wrapper.PullAndAssertDataRows({"a"});                                         \
    /* First relevant shadow row */                                               \
    wrapper.PullAndAssertShadowRows({{0, "a"}}, ExecutionState::HASMORE);         \
    /* Required to fetch empty input data */                                      \
    wrapper.PullAndAssertDataRows({});                                            \
    /* Second relevant shadow row */                                              \
    wrapper.PullAndAssertShadowRows({{0, "b"}, {1, "b"}}, ExecutionState::DONE);  \
    wrapper.StaysConstantAfterDone();                                             \
  }                                                                               \
                                                                                  \
  INSTANTIATE_TEST_CASE_P(TestName##Instanciated, TestName,                       \
                          testing::Range(static_cast<uint64_t>(0),                \
                                         static_cast<uint64_t>(std::pow(2, 3))));

// Section Third Pattern, 1 input, and alternating relevant irrelvant shadow rows
#define TEST_SHADOWROW_PATTERN_3(FetcherWrapper, TestName)                                      \
  class TestName : public testing::TestWithParam<uint64_t> {                                    \
   protected:                                                                                   \
    FetcherWrapper wrapper;                                                                     \
                                                                                                \
    void SetUp() override {                                                                     \
      SharedAqlItemBlockPtr baseBlock =                                                         \
          buildBlock<1>(wrapper.itemBlockManager(),                                             \
                        {{{R"("a")"}}, {{R"("a")"}}, {{R"("a")"}}, {{R"("b")"}}, {{R"("b")"}}}, \
                        {{1, 0}, {2, 1}, {3, 0}, {4, 1}});                                      \
      /* prepare the proxy */                                                                   \
      uint64_t splits = GetParam();                                                             \
      ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));                                  \
      auto toReturn = fetcherHelper::CutMyBlockIntoPieces(baseBlock, splits);                   \
      wrapper.shouldReturn(toReturn);                                                           \
    }                                                                                           \
  };                                                                                            \
                                                                                                \
  TEST_P(TestName, handle_shadow_rows) {                                                        \
    /* Fetch first data row */                                                                  \
    wrapper.PullAndAssertDataRows({"a"});                                                       \
                                                                                                \
    /* Fetch shadow rows */                                                                     \
    wrapper.PullAndAssertShadowRows({{0, "a"}, {1, "a"}}, ExecutionState::HASMORE);             \
                                                                                                \
    /* Now we need to fetch an empty list of Data rows */                                       \
    wrapper.PullAndAssertDataRows({});                                                          \
                                                                                                \
    /* Fetch "b" on two levels */                                                               \
    wrapper.PullAndAssertShadowRows({{0, "b"}, {1, "b"}}, ExecutionState::DONE);                \
                                                                                                \
    /* Assert we cannot get any more */                                                         \
    wrapper.StaysConstantAfterDone();                                                           \
  }                                                                                             \
                                                                                                \
  INSTANTIATE_TEST_CASE_P(TestName##Instanciated, TestName,                                     \
                          testing::Range(static_cast<uint64_t>(0),                              \
                                         static_cast<uint64_t>(std::pow(2, 4))));

// Section Foruth Pattern, 1 input, and alternating relevant irrelvant shadow rows
#define TEST_SHADOWROW_PATTERN_4(FetcherWrapper, TestName)                                      \
  class TestName : public testing::TestWithParam<uint64_t> {                                    \
   protected:                                                                                   \
    FetcherWrapper wrapper;                                                                     \
                                                                                                \
    void SetUp() override {                                                                     \
      SharedAqlItemBlockPtr baseBlock =                                                         \
          buildBlock<1>(wrapper.itemBlockManager(),                                             \
                        {{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}, {{R"("d")"}}, {{R"("d")"}}}, \
                        {{0, 0}, {4, 0}}); /* prepare the proxy*/                               \
      uint64_t splits = GetParam();                                                             \
      ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));                                  \
      auto toReturn = fetcherHelper::CutMyBlockIntoPieces(baseBlock, splits);                   \
      wrapper.shouldReturn(toReturn);                                                           \
    }                                                                                           \
  };                                                                                            \
                                                                                                \
  TEST_P(TestName, handle_shadow_rows) {                                                        \
    /*  No input data */                                                                        \
    wrapper.PullAndAssertDataRows({});                                                          \
    /* First relevant shadow row */                                                             \
    wrapper.PullAndAssertShadowRows({{0, "a"}}, ExecutionState::HASMORE);                       \
    /*  Block of data */                                                                        \
    wrapper.PullAndAssertDataRows({"b", "c", "d"});                                             \
    /* Final shadow row */                                                                      \
    wrapper.PullAndAssertShadowRows({{0, "d"}}, ExecutionState::DONE);                          \
    wrapper.StaysConstantAfterDone();                                                           \
  }                                                                                             \
                                                                                                \
  INSTANTIATE_TEST_CASE_P(TestName##Instanciated, TestName,                                     \
                          testing::Range(static_cast<uint64_t>(0),                              \
                                         static_cast<uint64_t>(std::pow(2, 4))));

// Section fifth Pattern, 1 input, 1 relevant, a set of irrelevant shadow rows
// followed by another input and 1 relevant, 1 irrelevant shadow row.

#define TEST_SHADOWROW_PATTERN_5(FetcherWrapper, TestName)                                 \
  class TestName : public testing::TestWithParam<uint64_t> {                               \
   protected:                                                                              \
    FetcherWrapper wrapper;                                                                \
                                                                                           \
    void SetUp() override {                                                                \
      SharedAqlItemBlockPtr baseBlock =                                                    \
          buildBlock<1>(wrapper.itemBlockManager(),                                        \
                        {{{R"("a")"}},                                                     \
                         {{R"("b")"}},                                                     \
                         {{R"("c")"}},                                                     \
                         {{R"("d")"}},                                                     \
                         {{R"("e")"}},                                                     \
                         {{R"("f")"}},                                                     \
                         {{R"("g")"}},                                                     \
                         {{R"("h")"}},                                                     \
                         {{R"("i")"}},                                                     \
                         {{R"("j")"}}},                                                    \
                        {{1, 0}, {2, 1}, {3, 2}, {4, 1}, {5, 2}, {7, 0}, {8, 1}, {9, 2}}); \
      uint64_t splits = GetParam();                                                        \
      ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));                             \
      auto toReturn = fetcherHelper::CutMyBlockIntoPieces(baseBlock, splits);              \
      wrapper.shouldReturn(toReturn);                                                      \
    }                                                                                      \
  };                                                                                       \
                                                                                           \
  TEST_P(TestName, handle_shadow_rows) {                                                   \
    /* The result should be always identical,*/                                            \
    /* it does not matter how the blocks */                                                \
    /* are splitted. We start with our single data row */                                  \
    wrapper.PullAndAssertDataRows({"a"});                                                  \
    /* We need to be able to uninterruptedly */                                            \
    /* fetch all irrelevant shadow rows */                                                 \
    wrapper.PullAndAssertShadowRows(                                                       \
        {{0, "b"}, {1, "c"}, {2, "d"}, {1, "e"}, {2, "f"}}, ExecutionState::HASMORE);      \
    /*Now another data block */                                                            \
    wrapper.PullAndAssertDataRows({"g"});                                                  \
    /* And the final block of ShadowRows */                                                \
    wrapper.PullAndAssertShadowRows({{0, "h"}, {1, "i"}, {2, "j"}}, ExecutionState::DONE); \
    wrapper.StaysConstantAfterDone();                                                      \
  }                                                                                        \
                                                                                           \
  INSTANTIATE_TEST_CASE_P(TestName##Instanciation, TestName,                               \
                          testing::Range(static_cast<uint64_t>(0),                         \
                                         static_cast<uint64_t>(std::pow(2, 9))));

// Section sixth Pattern, 10 input rows, no shadow rows.

#define TEST_SHADOWROW_PATTERN_6(FetcherWrapper, TestName)                    \
  class TestName : public testing::TestWithParam<uint64_t> {                  \
   protected:                                                                 \
    FetcherWrapper wrapper;                                                   \
                                                                              \
    void SetUp() override {                                                   \
      SharedAqlItemBlockPtr baseBlock =                                       \
          buildBlock<1>(wrapper.itemBlockManager(), {{{R"("a")"}},            \
                                                     {{R"("b")"}},            \
                                                     {{R"("c")"}},            \
                                                     {{R"("d")"}},            \
                                                     {{R"("e")"}},            \
                                                     {{R"("f")"}},            \
                                                     {{R"("g")"}},            \
                                                     {{R"("h")"}},            \
                                                     {{R"("i")"}},            \
                                                     {{R"("j")"}}});          \
      uint64_t splits = GetParam();                                           \
      ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));                \
      auto toReturn = fetcherHelper::CutMyBlockIntoPieces(baseBlock, splits); \
      wrapper.shouldReturn(toReturn);                                         \
    }                                                                         \
  };                                                                          \
                                                                              \
  TEST_P(TestName, handle_shadow_rows) {                                      \
    /* The result should be always identical,*/                               \
    /* it does not matter how the blocks */                                   \
    /* are splitted. We start with our single data row */                     \
    wrapper.PullAndAssertDataRows(                                            \
        {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"});                  \
    wrapper.PullAndAssertShadowRows({}, ExecutionState::DONE);                \
    wrapper.StaysConstantAfterDone();                                         \
  }                                                                           \
                                                                              \
  INSTANTIATE_TEST_CASE_P(TestName##Instanciation, TestName,                  \
                          testing::Range(static_cast<uint64_t>(0),            \
                                         static_cast<uint64_t>(std::pow(2, 9))));

}  // namespace fetcherHelper

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

#endif
