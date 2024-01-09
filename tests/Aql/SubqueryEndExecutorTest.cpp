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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"
#include "FixedOutputExecutionBlockMock.h"

#include "Aql/SubqueryEndExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::basics;

using RegisterSet = std::unordered_set<RegisterId>;

namespace {
RegisterInfos MakeBaseInfos(RegisterCount numRegs, size_t subqueryDepth = 2) {
  RegIdSet prototype{};
  for (RegisterId::value_t r = 0; r < numRegs; ++r) {
    prototype.emplace(r);
  }
  RegIdSetStack regsToKeep{};
  for (size_t i = 0; i <= subqueryDepth; ++i) {
    regsToKeep.push_back({prototype});
  }
  return RegisterInfos(RegIdSet{0}, RegIdSet{0}, numRegs, numRegs, {},
                       regsToKeep);
}

}  // namespace

class SubqueryEndExecutorTest : public AqlExecutorTestCase<false> {
 public:
  SubqueryEndExecutorInfos MakeExecutorInfos() {
    return SubqueryEndExecutorInfos{nullptr, monitor, 0, 0};
  }
};

TEST_F(SubqueryEndExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryEndExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds "
         "additional "
         "rows only.";
  EXPECT_EQ(SubqueryEndExecutor::Properties::allowsBlockPassthrough,
            ::arangodb::aql::BlockPassthrough::Disable)
      << "The block cannot be passThrough, as it increases the number of "
         "rows.";
}

TEST_F(SubqueryEndExecutorTest, count_shadow_rows_test) {
  // NOTE: This is a regression test for BTS-673
  std::deque<arangodb::aql::SharedAqlItemBlockPtr> inputData{};

  // The issue under test is to return too few results to
  // SubqueryStartExecutor including higher level shadow rows,which forces the
  // SubqueryStartExecutor to correctly count the returned rows.
  inputData.push_back(buildBlock<2>(
      manager(),
      {{1, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {2, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {3, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {4, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {5, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {6, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}}},
      {{1, 0}, {2, 1}, {4, 0}, {6, 0}, {7, 1}, {9, 0}, {11, 0}, {13, 0}}));
  // After this block we have returned 2 level 1 shadowrows, and 3 level 0
  // shadowrows.

  inputData.push_back(buildBlock<2>(manager(),
                                    {{NoneEntry{}, NoneEntry{}},
                                     {6, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {7, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}}},
                                    {{0, 1}, {2, 0}, {4, 0}, {5, 1}}));

  MockTypedNode inputNode{fakedQuery->plan(), ExecutionNodeId{1},
                          ExecutionNode::FILTER};
  FixedOutputExecutionBlockMock dependency{fakedQuery->rootEngine(), &inputNode,
                                           std::move(inputData)};
  MockTypedNode sqNode{fakedQuery->plan(), ExecutionNodeId{42},
                       ExecutionNode::SUBQUERY_END};
  ExecutionBlockImpl<SubqueryEndExecutor> testee{fakedQuery->rootEngine(),
                                                 &sqNode, MakeBaseInfos(2, 3),
                                                 MakeExecutorInfos()};
  testee.addDependency(&dependency);
  size_t mainQuerySoftLimit = 100;
  // MainQuery (SoftLimit 100)
  AqlCallStack callStack{AqlCallList{
      AqlCall{0, false, mainQuerySoftLimit, AqlCall::LimitType::SOFT}}};
  // outer subquery (SoftLimit 10)
  size_t subQuerySoftLimit = 10;
  callStack.pushCall(AqlCallList{
      AqlCall{0, false, subQuerySoftLimit, AqlCall::LimitType::SOFT},
      AqlCall{0, false, subQuerySoftLimit, AqlCall::LimitType::SOFT}});
  size_t numCalls = 0;

  dependency.setExecuteEnterHook(
      [&numCalls, mainQuerySoftLimit,
       subQuerySoftLimit](AqlCallStack const& stack) {
        auto mainQCall = stack.getCallAtDepth(2);
        auto subQCall = stack.getCallAtDepth(1);
        ASSERT_FALSE(mainQCall.needSkipMore());
        ASSERT_FALSE(subQCall.needSkipMore());
        if (numCalls == 0) {
          // Call with the original limits, SubqueryStart does not reduce it.
          ASSERT_EQ(mainQCall.getLimit(), mainQuerySoftLimit);
          ASSERT_EQ(subQCall.getLimit(), subQuerySoftLimit);
        } else if (numCalls == 1) {
          // We have returned some rows of each in the block before. They need
          // to be accounted
          ASSERT_EQ(mainQCall.getLimit(), mainQuerySoftLimit - 2);
          ASSERT_EQ(subQCall.getLimit(), subQuerySoftLimit - 3);
        } else {
          // Should not be called thrice.
          ASSERT_TRUE(false);
        }
        numCalls++;
      });

  auto [state, skipped, block] = testee.execute(callStack);

  ASSERT_EQ(numCalls, 2);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(block->numRows(), 12);
}
