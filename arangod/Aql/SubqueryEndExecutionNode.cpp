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

#include "SubqueryEndExecutionNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SubqueryEndExecutor.h"
#include "Basics/VelocyPackHelper.h"
#include "Meta/static_assert_size.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
bool CompareVariables(Variable const* mine, Variable const* yours) {
  if (mine == nullptr || yours == nullptr) {
    return mine == nullptr && yours == nullptr;
  }
  return mine->isEqualTo(*yours);
}
}  // namespace

SubqueryEndNode::SubqueryEndNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable", true)),
      _outVariable(Variable::varFromVPack(plan->getAst(), base, "outVariable")) {}

SubqueryEndNode::SubqueryEndNode(ExecutionPlan* plan, ExecutionNodeId id,
                                 Variable const* inVariable, Variable const* outVariable)
    : ExecutionNode(plan, id),
      _inVariable(inVariable),
      _outVariable(outVariable) {
  // _inVariable might be nullptr
  TRI_ASSERT(_outVariable != nullptr);
}

void SubqueryEndNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                         std::unordered_set<ExecutionNode const*>& seen) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  if (_inVariable != nullptr) {
    nodes.add(VPackValue("inVariable"));
    _inVariable->toVelocyPack(nodes);
  }

  nodes.close();
}

std::unique_ptr<ExecutionBlock> SubqueryEndNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const {

  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto inputRegisters = RegIdSet{};
  auto outputRegisters = RegIdSet{};

  auto inReg = variableToRegisterOptionalId(_inVariable);
  if (inReg != RegisterPlan::MaxRegisterId) {
    inputRegisters.emplace(inReg);
  }
  auto outReg = variableToRegisterId(_outVariable);
  outputRegisters.emplace(outReg);

  auto registerInfos = createRegisterInfos(std::move(inputRegisters), std::move(outputRegisters));

  auto const& vpackOptions = engine.getQuery().vpackOptions();
  auto executorInfos =
      SubqueryEndExecutorInfos(&vpackOptions, engine.getQuery().resourceMonitor(), inReg, outReg);

  return std::make_unique<ExecutionBlockImpl<SubqueryEndExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* SubqueryEndNode::clone(ExecutionPlan* plan, bool withDependencies,
                                      bool withProperties) const {
  auto outVariable = _outVariable;
  auto inVariable = _inVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
    if (inVariable != nullptr) {
      inVariable = plan->getAst()->variables()->createVariable(inVariable);
    }
  }
  auto c = std::make_unique<SubqueryEndNode>(plan, _id, inVariable, outVariable);

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

void SubqueryEndNode::replaceOutVariable(Variable const* var) {
  _outVariable = var;
}

CostEstimate SubqueryEndNode::estimateCost() const {
  TRI_ASSERT(_dependencies.size() == 1);

  CostEstimate estimate = _dependencies.at(0)->getCost();

  // Restore the nrItems that were saved at the corresponding SubqueryStartNode.
  estimate.restoreEstimatedNrItems();

  estimate.estimatedCost += estimate.estimatedNrItems;

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
    if (!CompareVariables(_outVariable, p._outVariable) ||
        !CompareVariables(_inVariable, p._inVariable)) {
      // One of the variables does not match
      return false;
    }
    return ExecutionNode::isEqualTo(p);
  } catch (const std::bad_cast&) {
    TRI_ASSERT(false);
    return false;
  }
}

// NOTE: A SubqueryEndNode should never be asked whether its a modification
//       node, as this information is supposed to be used in optimizer rules,
//       and subquery splicing runs as the *last* optimizer rule in any case.
//
//       If this assumption is changed, you will need to make an implementation
//       of this function that makes sense (for example by deriving the isModificationNode
//       from the SubqueryNode).
bool SubqueryEndNode::isModificationNode() const {
  TRI_ASSERT(false);
  return false;
}
