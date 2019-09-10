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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

SubqueryStartNode::SubqueryStartNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {}


/// @brief toVelocyPack, for SubqueryStartNode
void SubqueryStartNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("isConst", VPackValue(const_cast<SubqueryStartNode*>(this)->isConst()));

  // And add it:
  nodes.close();
}

/// @brief invalidate the cost estimation for the node and its dependencies
void SubqueryStartNode::invalidateCost() {
  ExecutionNode::invalidateCost();
}

bool SubqueryStartNode::isConst() {
  // TODO: Might be obsolete
  return false;
}

bool SubqueryStartNode::mayAccessCollections() {
  if (_plan->getAst()->functionsMayAccessDocuments()) {
    // if the query contains any calls to functions that MAY access any
    // document, then we count this as a "yes"
    return true;
  }

  return false;
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SubqueryStartNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {
  // TODO: fill me in
  TRI_ASSERT(false);

  return nullptr;
}

ExecutionNode* SubqueryStartNode::clone(ExecutionPlan* plan, bool withDependencies,
                                   bool withProperties) const {
  auto c = std::make_unique<SubqueryStartNode>(plan, _id);
  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief whether or not the subquery is a data-modification operation
bool SubqueryStartNode::isModificationSubquery() const {
  return false;
}

/// @brief estimateCost
CostEstimate SubqueryStartNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());

  // TODO: Fill me in
  CostEstimate estimate = _dependencies.at(0)->getCost();
  return estimate;
}

bool SubqueryStartNode::isDeterministic() {
  // TODO: ?

  return false;
}

} // namespace aql
} // namespace arangodb
