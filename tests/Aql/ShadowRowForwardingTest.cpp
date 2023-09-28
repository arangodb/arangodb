////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Aql/AqlExecutorTestCase.h"

#include "Aql/FilterExecutor.h"
#include "Aql/SubqueryStartExecutor.h"
#include "Aql/SubqueryEndExecutor.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

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
  return RegisterInfos({}, {}, numRegs, numRegs, {}, regsToKeep);
}
}  // namespace

constexpr bool enableQueryTrace = false;
struct ShadowRowForwardingTest : AqlExecutorTestCase<enableQueryTrace> {};

TEST_F(ShadowRowForwardingTest, subqueryStart) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1, 3),
                                          MakeBaseInfos(1, 3),
                                          ExecutionNode::SUBQUERY_START)
      .setInputSubqueryDepth(2)
      .setInputValue(
          {
              {R"("outer shadow row")"},
              {R"("data row")"},
              {R"("inner shadow row")"},
              {R"("outer shadow row")"},
          },
          {
              {0, 1},
              // {1, data row}
              {2, 0},
              {3, 1},
          })
      //.setInputSplit({})
      .setCallStack(AqlCallStack{
          AqlCallList{AqlCall{}, AqlCall{}},
          AqlCallList{AqlCall{}, AqlCall{}},
          AqlCallList{AqlCall{0, true, AqlCall::Infinity{}}},
          AqlCallList{AqlCall{}, AqlCall{}},
      })
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::HASMORE)
      .expectOutput(
          {0},
          {
              {R"("outer shadow row")"},
              // {R"("data row")"},
              // {R"("data row")"},  // this is now a relevant shadow row
              // {R"("inner shadow row")"},
              // {R"("outer shadow row")"},
          },
          {
              {0, 2},
              // // {1, data row}
              // {2, 0},
              // {3, 1},
              // {4, 2},
          })
      .expectSkipped(0, 0, 0, 0)
      .run();
};
