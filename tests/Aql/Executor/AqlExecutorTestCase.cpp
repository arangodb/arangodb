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
/// @author Markus Pfeiffer
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "AqlExecutorTestCase.h"
#include "Aql/ExecutionNode/DistributeConsumerNode.h"

using namespace arangodb::tests::aql;

template<bool enableQueryTrace>
AqlExecutorTestCase<enableQueryTrace>::AqlExecutorTestCase(Scheduler* scheduler)
    : fakedQuery{_server->createFakeQuery(scheduler, enableQueryTrace)} {
  auto engine = std::make_unique<ExecutionEngine>(
      0, *fakedQuery, manager(),
      std::make_shared<SharedQueryState>(_server->server(), scheduler));
  if constexpr (enableQueryTrace) {
    Logger::QUERIES.setLogLevel(LogLevel::DEBUG);
  }
}

template<bool enableQueryTrace>
AqlExecutorTestCase<enableQueryTrace>::AqlExecutorTestCase()
    : AqlExecutorTestCase(SchedulerFeature::SCHEDULER) {}

template<bool enableQueryTrace>
AqlExecutorTestCase<enableQueryTrace>::~AqlExecutorTestCase() {
  if constexpr (enableQueryTrace) {
    Logger::QUERIES.setLogLevel(LogLevel::INFO);
  }
}

template<bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::generateNodeDummy(
    ExecutionNode::NodeType type) -> ExecutionNode* {
  auto dummy = std::make_unique<MockTypedNode>(
      const_cast<arangodb::aql::ExecutionPlan*>(fakedQuery->plan()),
      ExecutionNodeId{_execNodes.size()}, type);
  auto res = dummy.get();
  _execNodes.emplace_back(std::move(dummy));
  return res;
}

template<bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::generateScatterNodeDummy()
    -> ScatterNode* {
  auto dummy = std::make_unique<ScatterNode>(
      const_cast<arangodb::aql::ExecutionPlan*>(fakedQuery->plan()),
      ExecutionNodeId{_execNodes.size()}, ScatterNode::ScatterType::SERVER);
  auto res = dummy.get();
  _execNodes.emplace_back(std::move(dummy));
  return res;
}

template<bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::generateMutexNodeDummy()
    -> MutexNode* {
  auto dummy = std::make_unique<MutexNode>(
      const_cast<arangodb::aql::ExecutionPlan*>(fakedQuery->plan()),
      ExecutionNodeId{_execNodes.size()});
  auto res = dummy.get();
  _execNodes.emplace_back(std::move(dummy));
  return res;
}
template<bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::generateDistributeConsumerNode(
    std::string distributeId) -> DistributeConsumerNode* {
  auto dummy = std::make_unique<DistributeConsumerNode>(
      const_cast<arangodb::aql::ExecutionPlan*>(fakedQuery->plan()),
      ExecutionNodeId{_execNodes.size()}, std::move(distributeId));
  auto res = dummy.get();
  _execNodes.emplace_back(std::move(dummy));
  return res;
}

template<bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::manager() const
    -> AqlItemBlockManager& {
  return fakedQuery->rootEngine()->itemBlockManager();
}

template<bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::query() const
    -> std::shared_ptr<arangodb::aql::Query> {
  return fakedQuery;
}

template class ::arangodb::tests::aql::AqlExecutorTestCase<true>;
template class ::arangodb::tests::aql::AqlExecutorTestCase<false>;
