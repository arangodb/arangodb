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

auto generateCallStack(size_t nestedSubqueryLevels, size_t indexWithoutContinueCall) -> AqlCallStack {
  // MainQuery never has continue call
  AqlCallStack stack{AqlCallList{AqlCall{}}};
  for (size_t i = 0; i < nestedSubqueryLevels; ++i) {
    if (i == indexWithoutContinueCall) {
      stack.pushCall(AqlCallList{AqlCall{}}); // no continue call
    } else {
      stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
    }
  }
  return stack;
}

}  // namespace

constexpr bool enableQueryTrace = false;
struct ShadowRowForwardingTest : AqlExecutorTestCase<enableQueryTrace> {
  arangodb::ResourceMonitor monitor{global};
  auto MakeSubqueryEndExecutorInfos(RegisterId inputRegister)
      -> SubqueryEndExecutor::Infos {
    auto const outputRegister =
        RegisterId{static_cast<RegisterId::value_t>(inputRegister.value() + 1)};

    return SubqueryEndExecutor::Infos(nullptr, monitor, inputRegister,
                                      outputRegister);
  }
};

TEST_F(ShadowRowForwardingTest, subqueryStart1) {
  auto const test = [&](SplitType splitType) {
    SCOPED_TRACE("with split type " + to_string(splitType));

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
        .setInputSplitType(splitType)
        .setCallStack(generateCallStack(3, 1))
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

  test(std::monostate{});
  test(std::size_t{1});
  test(std::vector<std::size_t>{1, 3});
};

TEST_F(ShadowRowForwardingTest, subqueryEnd1) {
  auto const test = [&](SplitType splitType) {
    SCOPED_TRACE("with split type " + to_string(splitType));

    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryEndExecutor>(MakeBaseInfos(1, 3),
                                          MakeSubqueryEndExecutorInfos(1),
                                          ExecutionNode::SUBQUERY_END)
        .setInputSubqueryDepth(3)
        .setInputValue(
            {
                {R"("outer shadow row")"},
                {R"("relevant shadow row")"},
                {R"("inner shadow row")"},
                {R"("outer shadow row")"},
            },
            {
                {0, 2},
                {1, 0},
                {2, 1},
                {3, 2},
            })
        .setInputSplitType(splitType)
        .setCallStack(generateCallStack(2, 1))
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput(
            {0},
            {
                {R"("outer shadow row")"},
                // {R"([])"}, // data row (previously relevant shadow row)
                // {R"("inner shadow row")"},
                // {R"("outer shadow row")"},
            },
            {
                {0, 1},
                // // {1, data row}
                // {2, 0},
                // {3, 1},
            })
        .expectSkipped(0, 0, 0)
        .run();
  };

  test(std::monostate{});
  test(std::size_t{1});
  test(std::vector<std::size_t>{1, 3});
};
