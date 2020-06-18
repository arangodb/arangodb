////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"

#include "Aql/IdExecutor.h"
#include "Aql/UnsortedGatherExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

enum class ExecutorType { UNSORTED };
auto dependencyNumber = testing::Values(1, 2, 3);

using CommonParameter = std::tuple<ExecutorType, size_t>;

class CommonGatherExecutorTest : public AqlExecutorTestCaseWithParam<CommonParameter> {
 protected:
  CommonGatherExecutorTest() {}

  auto getExecutor() -> std::unique_ptr<ExecutionBlock> {
    // TODO inject dependencies;
    return buildExecutor();
  }

 private:
  auto getType() -> ExecutorType {
    auto const& [type, clients] = GetParam();
    return type;
  }

  auto getClients() -> size_t {
    auto const& [type, clients] = GetParam();
    return clients;
  }

  auto buildRegisterInfos() -> RegisterInfos {
    return RegisterInfos(RegIdSet{}, {}, 1, 1, {}, {RegIdSet{0}});
  }

  auto buildExecutor() -> std::unique_ptr<ExecutionBlock> {
    auto regInfos = buildRegisterInfos();
    switch (getType()) {
      case ExecutorType::UNSORTED:
        return unsortedExecutor(std::move(regInfos));
      default:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
  }

  auto unsortedExecutor(RegisterInfos&& regInfos) -> std::unique_ptr<ExecutionBlock> {
    IdExecutorInfos execInfos{false};
    return std::make_unique<ExecutionBlockImpl<UnsortedGatherExecutor>>(
        fakedQuery->rootEngine(), generateNodeDummy(ExecutionNode::GATHER),
        std::move(regInfos), std::move(execInfos));
  }
};

INSTANTIATE_TEST_CASE_P(CommonGatherTests, CommonGatherExecutorTest,
                        ::testing::Combine(::testing::Values(ExecutorType::UNSORTED),
                                           dependencyNumber));

TEST_P(CommonGatherExecutorTest, get_all) {}

}  // namespace arangodb::tests::aql