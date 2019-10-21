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

#include "Aql/SubqueryStartExecutionNode.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SubqueryStartExecutor.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

SubqueryStartNode::SubqueryStartNode(ExecutionPlan* plan,
                                     arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {}

CostEstimate SubqueryStartNode::estimateCost() const {
  TRI_ASSERT(!_dependencies.empty());

  CostEstimate estimate = _dependencies.at(0)->getCost();
  return estimate;
}

void SubqueryStartNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                           std::unordered_set<ExecutionNode const*>& seen) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);
  nodes.close();
}

std::unique_ptr<ExecutionBlock> SubqueryStartNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>();

  // The const_cast has been taken from previous implementation.
  ExecutorInfos infos(inputRegisters, outputRegisters,
                      getRegisterPlan()->nrRegs[previousNode->getDepth()],
                      getRegisterPlan()->nrRegs[getDepth()], getRegsToClear(),
                      calcRegsToKeep());
  return std::make_unique<ExecutionBlockImpl<SubqueryStartExecutor>>(&engine, this,
                                                                     std::move(infos));
}

ExecutionNode* SubqueryStartNode::clone(ExecutionPlan* plan, bool withDependencies,
                                        bool withProperties) const {
  auto c = std::make_unique<SubqueryStartNode>(plan, _id);
  return cloneHelper(std::move(c), withDependencies, withProperties);
}

bool SubqueryStartNode::isEqualTo(ExecutionNode const& other) const {
  try {
    SubqueryStartNode const& p = dynamic_cast<SubqueryStartNode const&>(other);
    return ExecutionNode::isEqualTo(p);
  } catch (const std::bad_cast&) {
    return false;
  }
}

}  // namespace aql
}  // namespace arangodb
