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

#include "EnumerateCollectionNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/EnumerateCollectionExecutor.h"
#include "Aql/Expression.h"
#include "Aql/Projections.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterPlan.h"

using namespace arangodb;
using namespace arangodb::aql;

EnumerateCollectionNode::EnumerateCollectionNode(
    ExecutionPlan* plan, ExecutionNodeId id, aql::Collection const* collection,
    Variable const* outVariable, bool random, IndexHint const& hint)
    : ExecutionNode(plan, id),
      DocumentProducingNode(outVariable),
      CollectionAccessingNode(collection),
      _random(random),
      _hint(hint) {}

EnumerateCollectionNode::EnumerateCollectionNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      DocumentProducingNode(plan, base),
      CollectionAccessingNode(plan, base),
      _random(base.get("random").getBoolean()),
      _hint(base) {}

ExecutionNode::NodeType EnumerateCollectionNode::getType() const {
  return ENUMERATE_COLLECTION;
}

size_t EnumerateCollectionNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}

IndexHint const& EnumerateCollectionNode::hint() const { return _hint; }

/// @brief doToVelocyPack, for EnumerateCollectionNode
void EnumerateCollectionNode::doToVelocyPack(velocypack::Builder& builder,
                                             unsigned flags) const {
  builder.add("random", VPackValue(_random));

  _hint.toVelocyPack(builder);

  // add outvariable and projection
  DocumentProducingNode::toVelocyPack(builder, flags);

  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> EnumerateCollectionNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  // check which variables are used by the node's post-filter
  std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;

  if (hasFilter()) {
    VarSet inVars;
    filter()->variables(inVars);

    filterVarsToRegs.reserve(inVars.size());

    for (auto& var : inVars) {
      TRI_ASSERT(var != nullptr);
      auto regId = variableToRegisterId(var);
      filterVarsToRegs.emplace_back(var->id, regId);
    }
  }

  RegisterId outputRegister = variableToRegisterId(_outVariable);

  auto outputRegisters = RegIdSet{};

  auto const& p = projections();
  if (!p.empty()) {
    // projections. no need to produce the full document.
    // instead create one register per projection.
    for (size_t i = 0; i < p.size(); ++i) {
      Variable const* var = p[i].variable;
      if (var == nullptr) {
        // the output register can be a nullptr if the "optimize-projections"
        // rule was not executed (potentially because it was disabled).
        continue;
      }
      TRI_ASSERT(var != nullptr);
      auto regId = variableToRegisterId(var);
      filterVarsToRegs.emplace_back(var->id, regId);
      outputRegisters.emplace(regId);
    }
    // we should either have as many output registers set as there are
    // projections, or none. the latter can happen if the "optimize-projections"
    // rule did not (yet) run
    TRI_ASSERT(outputRegisters.empty() || outputRegisters.size() == p.size());
    // in case we do not have any output registers for the projections,
    // we must write them to the main output register, in a velocypack
    // object.
    // this will be handled below by adding the main output register.
  }
  if (outputRegisters.empty() && isProduceResult()) {
    outputRegisters.emplace(outputRegister);
  }
  TRI_ASSERT(!outputRegisters.empty() || !isProduceResult());

  auto registerInfos = createRegisterInfos({}, std::move(outputRegisters));
  auto executorInfos = EnumerateCollectionExecutorInfos(
      outputRegister, engine.getQuery(), collection(), _outVariable,
      isProduceResult(), filter(), projections(), std::move(filterVarsToRegs),
      _random, doCount(), canReadOwnWrites());
  return std::make_unique<ExecutionBlockImpl<EnumerateCollectionExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* EnumerateCollectionNode::clone(ExecutionPlan* plan,
                                              bool withDependencies) const {
  auto c = std::make_unique<EnumerateCollectionNode>(
      plan, _id, collection(), _outVariable, _random, _hint);

  CollectionAccessingNode::cloneInto(*c);
  DocumentProducingNode::cloneInto(plan, *c);

  return cloneHelper(std::move(c), withDependencies);
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void EnumerateCollectionNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  DocumentProducingNode::replaceVariables(replacements);
}

void EnumerateCollectionNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable,
    size_t /*index*/) {
  DocumentProducingNode::replaceAttributeAccess(self, searchVariable, attribute,
                                                replaceVariable);
}

void EnumerateCollectionNode::setRandom() { _random = true; }

bool EnumerateCollectionNode::isDeterministic() {
  return !_random && (canReadOwnWrites() == ReadOwnWrites::no);
}

void EnumerateCollectionNode::getVariablesUsedHere(VarSet& vars) const {
  if (hasFilter()) {
    Ast::getReferencedVariables(filter()->node(), vars);
    // need to unset the output variable that we produce ourselves,
    // otherwise the register planning runs into trouble. the register
    // planning's assumption is that all variables that are used in a
    // node must also be used later.
    vars.erase(outVariable());

    for (size_t i = 0; i < _projections.size(); ++i) {
      if (_projections[i].variable != nullptr) {
        vars.erase(_projections[i].variable);
      }
    }
  }
}

std::vector<Variable const*> EnumerateCollectionNode::getVariablesSetHere()
    const {
  return DocumentProducingNode::getVariablesSetHere();
}

void EnumerateCollectionNode::setProjections(Projections projections) {
  DocumentProducingNode::setProjections(std::move(projections));
}

bool EnumerateCollectionNode::isProduceResult() const {
  if (hasFilter()) {
    return true;
  }
  auto const& p = projections();
  if (p.empty()) {
    return isVarUsedLater(_outVariable);
  }
  // check output registers of projections
  size_t found = 0;
  for (size_t i = 0; i < p.size(); ++i) {
    Variable const* var = p[i].variable;
    // the output register can be a nullptr if the "optimize-projections"
    // rule was not executed (potentially because it was disabled).
    if (var != nullptr) {
      ++found;
      if (isVarUsedLater(var)) {
        return true;
      }
    }
  }
  if (found == 0) {
    return isVarUsedLater(_outVariable);
  }
  return false;
}

/// @brief the cost of an enumerate collection node is a multiple of the cost of
/// its unique dependency
CostEstimate EnumerateCollectionNode::estimateCost() const {
  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  if (trx.status() != transaction::Status::RUNNING) {
    return CostEstimate::empty();
  }

  TRI_ASSERT(!_dependencies.empty());
  CostEstimate estimate = _dependencies.at(0)->getCost();
  auto estimatedNrItems =
      collection()->count(&trx, transaction::CountType::kTryCache);
  if (_random) {
    // we retrieve at most one random document from the collection.
    // so the estimate is at most 1
    // estimatedNrItems = 1;
    // Unnecessary to set, we just leave estimate.estimatedNrItems as it is.
  } else if (!doCount()) {
    // if "count" mode is active, the estimated number of items from above
    // must not be multiplied with the number of items in this collection
    estimate.estimatedNrItems *= estimatedNrItems;
  }
  // We do a full collection scan for each incoming item.
  // random iteration is slightly more expensive than linear iteration
  // we also penalize each EnumerateCollectionNode slightly (and do not
  // do the same for IndexNodes) so IndexNodes will be preferred
  estimate.estimatedCost += estimate.estimatedNrItems *
                                (_random ? 1.005 : (hasFilter() ? 1.25 : 1.0)) +
                            1.0;

  return estimate;
}

AsyncPrefetchEligibility EnumerateCollectionNode::canUseAsyncPrefetching()
    const noexcept {
  return DocumentProducingNode::canUseAsyncPrefetching();
}
