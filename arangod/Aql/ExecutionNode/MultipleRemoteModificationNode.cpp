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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "MultipleRemoteModificationNode.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/Executor/ModificationExecutorHelpers.h"
#include "Aql/Executor/ModificationExecutorInfos.h"
#include "Aql/Executor/MultipleRemoteModificationExecutor.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/types.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;

MultipleRemoteModificationNode::MultipleRemoteModificationNode(
    ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
    ModificationOptions const& options, Variable const* inVariable,
    Variable const* outVariable, Variable const* OLD, Variable const* NEW)
    : ExecutionNode(plan, id),
      CollectionAccessingNode(collection),
      _inVariable(inVariable),
      _outVariable(outVariable),
      _outVariableOld(OLD),
      _outVariableNew(NEW),
      _options(options) {}

/// @brief creates corresponding MultipleRemoteModificationNode
std::unique_ptr<ExecutionBlock> MultipleRemoteModificationNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();

  TRI_ASSERT(previousNode != nullptr);

  RegisterId in = variableToRegisterOptionalId(_inVariable);
  RegisterId out = variableToRegisterOptionalId(_outVariable);
  RegisterId outputNew = variableToRegisterOptionalId(_outVariableNew);
  RegisterId outputOld = variableToRegisterOptionalId(_outVariableOld);

  OperationOptions options = ModificationExecutorHelpers::convertOptions(
      _options, _outVariableNew, _outVariableOld);

  auto readableInputRegisters = RegIdSet{};
  if (in.isValid()) {
    readableInputRegisters.emplace(in);
  }
  auto writableOutputRegisters = RegIdSet{};
  if (out.isValid()) {
    writableOutputRegisters.emplace(out);
  }
  if (outputNew.isValid()) {
    writableOutputRegisters.emplace(outputNew);
  }
  if (outputOld.isValid()) {
    writableOutputRegisters.emplace(outputOld);
  }

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  auto executorInfos = MultipleRemoteModificationInfos(
      &engine, in, outputNew, outputOld, out, _plan->getAst()->query(),
      std::move(options), collection(),
      ConsultAqlWriteFilter(_options.consultAqlWriteFilter),
      IgnoreErrors(_options.ignoreErrors),
      IgnoreDocumentNotFound(_options.ignoreDocumentNotFound),
      this->hasParent(), this->_options.exclusive);
  return std::make_unique<
      ExecutionBlockImpl<MultipleRemoteModificationExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief doToVelocyPack, for MultipleRemoteModificationNode
void MultipleRemoteModificationNode::doToVelocyPack(VPackBuilder& nodes,
                                                    unsigned flags) const {
  // add collection information
  CollectionAccessingNode::toVelocyPack(nodes, flags);

  // add out variables
  bool isAnyVarUsedLater = false;
  if (_outVariableOld != nullptr) {
    nodes.add(VPackValue("outVariableOld"));
    _outVariableOld->toVelocyPack(nodes);
    isAnyVarUsedLater |= isVarUsedLater(_outVariableOld);
  }
  if (_outVariableNew != nullptr) {
    nodes.add(VPackValue("outVariableNew"));
    _outVariableNew->toVelocyPack(nodes);
    isAnyVarUsedLater |= isVarUsedLater(_outVariableNew);
  }

  if (_inVariable != nullptr) {
    nodes.add(VPackValue("inVariable"));
    _inVariable->toVelocyPack(nodes);
  }

  if (_outVariable != nullptr) {
    nodes.add(VPackValue("outVariable"));
    _outVariable->toVelocyPack(nodes);
    isAnyVarUsedLater |= isVarUsedLater(_outVariable);
  }
  nodes.add(StaticStrings::ProducesResult, VPackValue(isAnyVarUsedLater));
  nodes.add(VPackValue("modificationFlags"));
  _options.toVelocyPack(nodes);
}

/// @brief estimateCost
CostEstimate MultipleRemoteModificationNode::estimateCost() const {
  size_t length = estimateListLength(_plan, _inVariable);

  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems *= length;
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

std::vector<Variable const*>
MultipleRemoteModificationNode::getVariablesSetHere() const {
  std::vector<Variable const*> vec;

  if (_outVariable) {
    vec.push_back(_outVariable);
  }
  if (_outVariableNew) {
    vec.push_back(_outVariableNew);
  }
  if (_outVariableOld) {
    vec.push_back(_outVariableOld);
  }

  return vec;
}

void MultipleRemoteModificationNode::getVariablesUsedHere(VarSet& vars) const {
  if (_inVariable) {
    vars.emplace(_inVariable);
  }
}

size_t MultipleRemoteModificationNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}
