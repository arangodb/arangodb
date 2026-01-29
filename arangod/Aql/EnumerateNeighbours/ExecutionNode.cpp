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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/EnumerateNeighbours/ExecutionNode.h"

namespace arangodb::aql::enumerate_neighbours {

ExecutionNode::ExecutionNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ::arangodb::aql::ExecutionNode(plan, id) {}

ExecutionNode::ExecutionNode(ExecutionPlan* plan,
                             arangodb::velocypack::Slice base)
    : ::arangodb::aql::ExecutionNode(plan, base) {}

ExecutionNode::~ExecutionNode() {}

std::unique_ptr<ExecutionBlock> ExecutionNode::createBlock(
    ExecutionEngine& engine) const {
  ADB_PROD_CRASH() << "implement ExecutionNode::createBlock";
}

::arangodb::aql::ExecutionNode* ExecutionNode::clone(
    ExecutionPlan* plan, bool withDependencies) const {
  ADB_PROD_CRASH() << "implement ExecutionNode::clone";
}

}  // namespace arangodb::aql::enumerate_neighbours
