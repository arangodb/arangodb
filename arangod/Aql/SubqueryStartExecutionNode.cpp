////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////


#include "Aql/Ast.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/SubqueryStartExecutionNode.h"
#include "Meta/static_assert_size.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>



namespace arangodb {
namespace aql {

SubqueryStartNode::SubqueryStartNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {}


CostEstimate SubqueryStartNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());

  CostEstimate estimate = _dependencies.at(0)->getCost();
  return estimate;
}

void SubqueryStartNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);
  nodes.close();
}

std::unique_ptr<ExecutionBlock> SubqueryStartNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {
  TRI_ASSERT(false);

  return nullptr;
}

ExecutionNode* SubqueryStartNode::clone(ExecutionPlan* plan, bool withDependencies,
                                   bool withProperties) const {
  auto c = std::make_unique<SubqueryStartNode>(plan, _id);
  return cloneHelper(std::move(c), withDependencies, withProperties);
}

bool SubqueryStartNode::isEqualTo(ExecutionNode const& other) const {
  // If this assertion fails, someone changed the size of SubqueryStartNode,
  // likely by adding or removing members, requiring this method to be updated.
  meta::details::static_assert_size<SubqueryStartNode, 456>();
  if (SubqueryStartNode const& p = dynamic_cast<SubqueryStartNode const*>(other)) {
    return ExecutionNode::isEqualTo(other);
  } else {
    return false;
  }
}

} // namespace aql
} // namespace arangodb

