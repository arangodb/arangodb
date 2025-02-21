////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/Executor/GroupedSortExecutor.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RowFetcherHelper.h"

#include "Aql/SortRegister.h"

#include "Aql/types.h"
#include "gtest/gtest.h"
#include <vector>

using namespace arangodb;
using namespace arangodb::aql;

class GroupedSortExecutorTest : public AqlExecutorTestCaseWithParam<SplitType> {
 public:
  auto registerInfos(RegIdSet registers) -> RegisterInfos {
    return RegisterInfos{registers,
                         {},
                         static_cast<uint16_t>(registers.size()),
                         static_cast<uint16_t>(registers.size()),
                         {},
                         RegIdSetStack{registers}};
  }
  auto getSortRegisters(std::vector<RegisterId> registers)
      -> std::vector<SortRegister> {
    std::vector<SortRegister> sortRegisters;
    for (auto const& id : registers) {
      sortRegisters.emplace_back(
          SortRegister{id, SortElement::create(&sortVar, true)});
    }
    return sortRegisters;
  }
  auto groupedSortExecutorInfos(std::vector<RegisterId> groupedRegisters,
                                std::vector<RegisterId> sortRegisters) {
    return GroupedSortExecutorInfos{getSortRegisters(sortRegisters),
                                    groupedRegisters, false, vpackOptions,
                                    monitor};
  }

 protected:
  velocypack::Options const* vpackOptions{&velocypack::Options::Defaults};
  Variable sortVar{"mySortVar", 0, false, monitor};
};

INSTANTIATE_TEST_CASE_P(GroupedSortExecutorTest, GroupedSortExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<1>,
                                          splitStep<2>));

TEST_P(GroupedSortExecutorTest, sorts_normally_when_nothing_is_grouped) {
  auto sortRegisterId = 0;
  makeExecutorTestHelper<1, 1>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId}),
          groupedSortExecutorInfos({}, {sortRegisterId}), ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{3}, {8}, {1009}, {832}, {-1}, {1}, {0}})
      .expectOutput({sortRegisterId}, {{-1}, {0}, {1}, {3}, {8}, {832}, {1009}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, does_nothing_when_no_sort_registry_is_given) {
  auto groupedRegisterId = 0;
  makeExecutorTestHelper<1, 1>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{3}, {8}, {1009}, {832}, {-1}, {1}, {0}})
      .expectOutput({groupedRegisterId},
                    {{3}, {8}, {1009}, {832}, {-1}, {1}, {0}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, sorts_in_groups) {
  auto sortRegisterId = 1;
  auto groupedRegisterId = 0;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {199, 2},
                      {199, 3},
                      {1, 1009},
                      {0, 832},
                      {0, 1},
                      {0, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1},
                                                          {2, 3},
                                                          {199, 2},
                                                          {199, 3},
                                                          {199, 8},
                                                          {1, 1009},
                                                          {0, 1},
                                                          {0, 832},
                                                          {0, 10001}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest,
       does_not_group_itself_but_assumes_that_rows_are_already_grouped) {
  auto sortRegisterId = 1;
  auto groupedRegisterId = 0;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {1, 1009},
                      {0, 832},
                      {199, 1},
                      {1, 1},
                      {199, 4}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1},
                                                          {2, 3},
                                                          {199, 8},
                                                          {1, 1009},
                                                          {0, 832},
                                                          {199, 1},
                                                          {1, 1},
                                                          {199, 4}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest,
       sorts_values_when_group_registry_is_same_as_sort_registry) {
  auto sortRegisterId = 0;
  makeExecutorTestHelper<1, 1>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId}),
          groupedSortExecutorInfos({}, {sortRegisterId}), ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{3}, {8}, {1009}, {832}, {-1}, {1}, {0}})
      .expectOutput({sortRegisterId}, {{-1}, {0}, {1}, {3}, {8}, {832}, {1009}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}
TEST_P(GroupedSortExecutorTest, ignores_non_sort_or_group_registry) {
  auto groupedRegisterId = 0;
  auto otherRegisterId = 1;
  auto sortRegisterId = 2;
  makeExecutorTestHelper<3, 3>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(
              RegIdSet{groupedRegisterId, otherRegisterId, sortRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 5, 3},
                      {2, 6, 1},
                      {199, 3, 8},
                      {199, 4, 2},
                      {199, 5, 3},
                      {1, 9, 1009},
                      {0, 87, 832},
                      {0, 50, 1},
                      {0, 78, 10001}})
      .expectOutput({groupedRegisterId, otherRegisterId, sortRegisterId},
                    {{2, 6, 1},
                     {2, 5, 3},
                     {199, 4, 2},
                     {199, 5, 3},
                     {199, 3, 8},
                     {1, 9, 1009},
                     {0, 50, 1},
                     {0, 87, 832},
                     {0, 78, 10001}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest,
       sorts_in_sort_register_for_several_group_registers) {
  auto groupedRegisterId_1 = 0;
  auto groupedRegisterId_2 = 1;
  auto sortRegisterId = 2;
  makeExecutorTestHelper<3, 3>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{groupedRegisterId_1, groupedRegisterId_2,
                                 sortRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId_1, groupedRegisterId_2},
                                   {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 5, 3},
                      {2, 5, 1},
                      {199, 5, 8},
                      {199, 4, 2},
                      {199, 5, 3},
                      {1, 50, 1009},
                      {0, 50, 832},
                      {0, 50, 1},
                      {0, 78, 10001}})
      .expectOutput({groupedRegisterId_1, groupedRegisterId_2, sortRegisterId},
                    {{2, 5, 1},
                     {2, 5, 3},
                     {199, 5, 8},
                     {199, 4, 2},
                     {199, 5, 3},
                     {1, 50, 1009},
                     {0, 50, 1},
                     {0, 50, 832},
                     {0, 78, 10001}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, sorts_via_all_sort_registers) {
  auto groupedRegisterId = 0;
  auto sortRegisterId_1 = 1;
  auto sortRegisterId_2 = 2;
  makeExecutorTestHelper<3, 3>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(
              RegIdSet{groupedRegisterId, sortRegisterId_1, sortRegisterId_2}),
          groupedSortExecutorInfos({groupedRegisterId},
                                   {sortRegisterId_1, sortRegisterId_2}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 5, 3},
                      {2, 5, 1},
                      {199, 5, 8},
                      {199, 4, 2},
                      {199, 5, 3},
                      {1, 50, 1009},
                      {0, 50, 832},
                      {0, 50, 1},
                      {0, 78, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId_1, sortRegisterId_2},
                    {{2, 5, 1},
                     {2, 5, 3},
                     {199, 4, 2},
                     {199, 5, 3},
                     {199, 5, 8},
                     {1, 50, 1009},
                     {0, 50, 1},
                     {0, 50, 832},
                     {0, 78, 10001}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, skip) {
  auto groupedRegisterId = 0;
  auto sortRegisterId = 1;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {199, 2},
                      {199, 3},
                      {1, 1009},
                      {0, 832},
                      {0, 1},
                      {0, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {{199, 2},
                                                          {199, 3},
                                                          {199, 8},
                                                          {1, 1009},
                                                          {0, 1},
                                                          {0, 832},
                                                          {0, 10001}})
      .setCall(AqlCall{2})
      .expectSkipped(2)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, hard_limit) {
  auto groupedRegisterId = 0;
  auto sortRegisterId = 1;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {199, 2},
                      {199, 3},
                      {1, 1009},
                      {0, 832},
                      {0, 1},
                      {0, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1}, {2, 3}})
      .setCall(AqlCall{0, false, 2, AqlCall::LimitType::HARD})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, soft_limit) {
  auto groupedRegisterId = 0;
  auto sortRegisterId = 1;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {199, 2},
                      {199, 3},
                      {1, 1009},
                      {0, 832},
                      {0, 1},
                      {0, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1}, {2, 3}})
      .setCall(AqlCall{0, false, 2, AqlCall::LimitType::SOFT})
      .expectSkipped(0)
      .expectedState(ExecutionState::HASMORE)
      .run();
}

TEST_P(GroupedSortExecutorTest, fullcount) {
  auto groupedRegisterId = 0;
  auto sortRegisterId = 1;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {199, 2},
                      {199, 3},
                      {1, 1009},
                      {0, 832},
                      {0, 1},
                      {0, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1}, {2, 3}})
      .setCall(AqlCall{0, true, 2, AqlCall::LimitType::HARD})
      .expectSkipped(7)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, skip_produce_fullcount) {
  auto groupedRegisterId = 0;
  auto sortRegisterId = 1;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {199, 2},
                      {199, 3},
                      {1, 1009},
                      {0, 832},
                      {0, 1},
                      {0, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {{199, 2}, {199, 3}})
      .setCall(AqlCall{2, true, 2, AqlCall::LimitType::HARD})
      .expectSkipped(7)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(GroupedSortExecutorTest, skip_too_much) {
  auto groupedRegisterId = 0;
  auto sortRegisterId = 1;
  makeExecutorTestHelper<2, 2>()
      .addConsumer<GroupedSortExecutor>(
          registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
          groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
          ExecutionNode::SORT)
      .setInputSplitType(GetParam())
      .setInputValue({{2, 3},
                      {2, 1},
                      {199, 8},
                      {199, 2},
                      {199, 3},
                      {1, 1009},
                      {0, 832},
                      {0, 1},
                      {0, 10001}})
      .expectOutput({groupedRegisterId, sortRegisterId}, {})
      .setCall(AqlCall{10, false})
      .expectSkipped(9)
      .expectedState(ExecutionState::DONE)
      .run();
}
