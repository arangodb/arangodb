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
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/NodeFinder.h"
#include "Aql/IdExecutor.h"
#include "Aql/Query.h"
#include "Aql/SubqueryEndExecutionNode.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

SubqueryEndNode::SubqueryEndNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _outVariable(Variable::varFromVPack(plan->getAst(), base, "outVariable")) {}

void SubqueryEndNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.close();
}

std::unique_ptr<ExecutionBlock> SubqueryEndNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {
  TRI_ASSERT(false);

  return nullptr;
}

ExecutionNode* SubqueryEndNode::clone(ExecutionPlan* plan, bool withDependencies,
                                   bool withProperties) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }
  auto c = std::make_unique<SubqueryEndNode>(plan, _id, outVariable);

  return cloneHelper(std::move(c), withDependencies, withProperties);
}


void SubqueryEndNode::replaceOutVariable(Variable const* var) {
  _outVariable = var;
}

CostEstimate SubqueryEndNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.size() == 1);

  CostEstimate estimate = _dependencies.at(0)->getCost();

  return estimate;
}

bool SubqueryEndNode::isEqualTo(SubqueryEndNode const& other)
{
  TRI_ASSERT(_outVariable); TRI_ASSERT(other->_outVariable);
  return ExecutionNode::isEqualTo(other) &&
    _outVariable->isEqualTo(*other->_outVariable);
}

} // namespace aql
} // namespace arangodb
