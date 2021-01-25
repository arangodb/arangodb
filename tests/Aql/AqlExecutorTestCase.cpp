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
/// @author Markus Pfeiffer
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "AqlExecutorTestCase.h"

using namespace arangodb::tests::aql;

template <bool enableQueryTrace>
AqlExecutorTestCase<enableQueryTrace>::AqlExecutorTestCase()
    : fakedQuery{_server->createFakeQuery(enableQueryTrace)} {
  auto engine = std::make_unique<ExecutionEngine>(0, *fakedQuery, manager(),
                                                  SerializationFormat::SHADOWROWS);
  /// TODO fakedQuery->setEngine(engine.release());
  if constexpr (enableQueryTrace) {
    Logger::QUERIES.setLogLevel(LogLevel::DEBUG);
  }
}

template <bool enableQueryTrace>
AqlExecutorTestCase<enableQueryTrace>::~AqlExecutorTestCase() {
  if constexpr (enableQueryTrace) {
    Logger::QUERIES.setLogLevel(LogLevel::INFO);
  }
}

template <bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::generateNodeDummy(ExecutionNode::NodeType type)
    -> ExecutionNode* {
  auto dummy =
      std::make_unique<MockTypedNode>(const_cast<arangodb::aql::ExecutionPlan*>(
                                          fakedQuery->plan()),
                                      ExecutionNodeId{_execNodes.size()}, type);
  auto res = dummy.get();
  _execNodes.emplace_back(std::move(dummy));
  return res;
}

template <bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::generateScatterNodeDummy() -> ScatterNode* {
  auto dummy = std::make_unique<ScatterNode>(
      const_cast<arangodb::aql::ExecutionPlan*>(fakedQuery->plan()),
      ExecutionNodeId{_execNodes.size()}, ScatterNode::ScatterType::SERVER);
  auto res = dummy.get();
  _execNodes.emplace_back(std::move(dummy));
  return res;
}

template <bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::manager() const -> AqlItemBlockManager& {
  return fakedQuery->rootEngine()->itemBlockManager();
}

template class ::arangodb::tests::aql::AqlExecutorTestCase<true>;
template class ::arangodb::tests::aql::AqlExecutorTestCase<false>;
