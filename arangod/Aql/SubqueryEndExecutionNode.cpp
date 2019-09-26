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

#include "Aql/SubqueryEndExecutionNode.h"
#include "Aql/SubqueryEndExecutor.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IdExecutor.h"
#include "Aql/NodeFinder.h"
#include "Aql/Query.h"
#include "Meta/static_assert_size.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

SubqueryEndNode::SubqueryEndNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _outVariable(Variable::varFromVPack(plan->getAst(), base, "outVariable")) {}

void SubqueryEndNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                         std::unordered_set<ExecutionNode const*>& seen) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.close();
}

std::unique_ptr<ExecutionBlock> SubqueryEndNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {
  transaction::Methods* trx = _plan->getAst()->query()->trx();
  TRI_ASSERT(trx != nullptr);

  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>();

  auto outVar = getRegisterPlan()->varInfo.find(_outVariable->id);
  TRI_ASSERT(outVar != getRegisterPlan()->varInfo.end());
  RegisterId outReg = outVar->second.registerId;
  outputRegisters->emplace(outReg);

  // The const_cast has been taken from previous implementation.
  SubqueryEndExecutorInfos infos(inputRegisters, outputRegisters,
                                 getRegisterPlan()->nrRegs[previousNode->getDepth()],
                                 getRegisterPlan()->nrRegs[getDepth()],
                                 getRegsToClear(), calcRegsToKeep(), trx, outReg);

  return std::make_unique<ExecutionBlockImpl<SubqueryEndExecutor>>(&engine, this,
                                                                   std::move(infos));
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
  TRI_ASSERT(_dependencies.size() == 1);

  CostEstimate estimate = _dependencies.at(0)->getCost();

  return estimate;
}

bool SubqueryEndNode::isEqualTo(ExecutionNode const& other) const {
  TRI_ASSERT(_outVariable != nullptr);
  if (other.getType() != getType()) {
    return false;
  }
  try {
    SubqueryEndNode const& p = dynamic_cast<SubqueryEndNode const&>(other);
    TRI_ASSERT(p._outVariable != nullptr);
    return ExecutionNode::isEqualTo(p) && _outVariable->isEqualTo(*(p._outVariable));
  } catch (const std::bad_cast&) {
    TRI_ASSERT(false);
    return false;
  }
}

}  // namespace aql
}  // namespace arangodb
