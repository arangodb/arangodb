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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "SingleRemoteOperationNode.h"

#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/ModificationExecutorHelpers.h"
#include "Aql/Executor/SingleRemoteModificationExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"

#include <string_view>
#include <type_traits>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;

SingleRemoteOperationNode::SingleRemoteOperationNode(
    ExecutionPlan* plan, ExecutionNodeId id, NodeType mode,
    bool replaceIndexNode, std::string const& key, Collection const* collection,
    ModificationOptions const& options, Variable const* in, Variable const* out,
    Variable const* OLD, Variable const* NEW)
    : ExecutionNode(plan, id),
      CollectionAccessingNode(collection),
      _replaceIndexNode(replaceIndexNode),
      _key(key),
      _mode(mode),
      _inVariable(in),
      _outVariable(out),
      _outVariableOld(OLD),
      _outVariableNew(NEW),
      _options(options) {
  if (_mode == NodeType::INDEX) {  // select
    TRI_ASSERT(!_key.empty());
    TRI_ASSERT(_inVariable == nullptr);
    TRI_ASSERT(_outVariable != nullptr);
    TRI_ASSERT(_outVariableOld == nullptr);
    TRI_ASSERT(_outVariableNew == nullptr);
  } else if (_mode == NodeType::REMOVE) {
    TRI_ASSERT(!_key.empty());
    TRI_ASSERT(_inVariable == nullptr);
    TRI_ASSERT(_outVariableNew == nullptr);
  } else if (_mode == NodeType::INSERT) {
    TRI_ASSERT(_key.empty());
  } else if (_mode != NodeType::UPDATE && _mode != NodeType::REPLACE) {
    TRI_ASSERT(false);
  }
}

// We probably do not need this, because the rule will only be used on the
// coordinator
SingleRemoteOperationNode::SingleRemoteOperationNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base), CollectionAccessingNode(plan, base) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "single remote operation node deserialization not implemented.");
}

/// @brief creates corresponding SingleRemoteOperationNode
std::unique_ptr<ExecutionBlock> SingleRemoteOperationNode::createBlock(
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

  auto executorInfos = SingleRemoteModificationInfos(
      &engine, in, outputNew, outputOld, out, _plan->getAst()->query(),
      std::move(options), collection(),
      ConsultAqlWriteFilter(_options.consultAqlWriteFilter),
      IgnoreErrors(_options.ignoreErrors),
      IgnoreDocumentNotFound(_options.ignoreDocumentNotFound), _key,
      this->hasParent(), this->_replaceIndexNode);

  if (_mode == NodeType::INDEX) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<IndexTag>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::INSERT) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Insert>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::REMOVE) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Remove>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::REPLACE) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Replace>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::UPDATE) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Update>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else if (_mode == NodeType::UPSERT) {
    return std::make_unique<
        ExecutionBlockImpl<SingleRemoteModificationExecutor<Upsert>>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else {
    TRI_ASSERT(false);
    return nullptr;
  }
}

/// @brief clone ExecutionNode recursively
ExecutionNode* SingleRemoteOperationNode::clone(ExecutionPlan* plan,
                                                bool withDependencies) const {
  auto n = std::make_unique<SingleRemoteOperationNode>(
      plan, _id, _mode, _replaceIndexNode, _key, collection(), _options,
      _inVariable, _outVariable, _outVariableOld, _outVariableNew);
  CollectionAccessingNode::cloneInto(*n);
  return cloneHelper(std::move(n), withDependencies);
}

/// @brief doToVelocyPack, for SingleRemoteOperationNode
void SingleRemoteOperationNode::doToVelocyPack(VPackBuilder& nodes,
                                               unsigned flags) const {
  CollectionAccessingNode::toVelocyPackHelperPrimaryIndex(nodes);

  // add collection information
  CollectionAccessingNode::toVelocyPack(nodes, flags);

  nodes.add("mode", VPackValue(ExecutionNode::getTypeString(_mode)));
  nodes.add("replaceIndexNode", VPackValue(_replaceIndexNode));

  if (!_key.empty()) {
    nodes.add("key", VPackValue(_key));
  }

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

  nodes.add("projections", VPackValue(VPackValueType::Array));
  // TODO: support projections?
  nodes.close();
}

/// @brief estimateCost
CostEstimate SingleRemoteOperationNode::estimateCost() const {
  CostEstimate estimate = _dependencies[0]->getCost();
  return estimate;
}

AsyncPrefetchEligibility SingleRemoteOperationNode::canUseAsyncPrefetching()
    const noexcept {
  return AsyncPrefetchEligibility::kDisableGlobally;
}

std::vector<Variable const*> SingleRemoteOperationNode::getVariablesSetHere()
    const {
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

void SingleRemoteOperationNode::getVariablesUsedHere(VarSet& vars) const {
  if (_inVariable) {
    vars.emplace(_inVariable);
  }
}

size_t SingleRemoteOperationNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}
