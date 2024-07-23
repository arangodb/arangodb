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

#include "SortNode.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/ConstrainedSortExecutor.h"
#include "Aql/Executor/SortExecutor.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortRegister.h"
#include "Aql/WalkerWorker.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/TemporaryStorageFeature.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

SortNode::SortNode(ExecutionPlan* plan, ExecutionNodeId id,
                   SortElementVector elements, bool stable)
    : ExecutionNode(plan, id),
      _elements(std::move(elements)),
      _stable(stable),
      _reinsertInCluster(true) {}

SortNode::SortNode(ExecutionPlan* plan, arangodb::velocypack::Slice base,
                   SortElementVector elements, bool stable)
    : ExecutionNode(plan, base),
      _elements(std::move(elements)),
      _stable(stable),
      _reinsertInCluster(true),
      _limit(VelocyPackHelper::getNumericValue<size_t>(base, "limit", 0)) {}

/// @brief doToVelocyPack, for SortNode
void SortNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  nodes.add(VPackValue("elements"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _elements) {
      it.toVelocyPack(nodes);
    }
  }
  nodes.add("stable", VPackValue(_stable));
  nodes.add("limit", VPackValue(_limit));
  nodes.add("strategy", VPackValue(sorterTypeName(sorterType())));
}

/// @brief simplifies the expressions of the sort node
/// this will sort expressions if they are constant
/// the method will return true if all sort expressions were removed after
/// simplification, and false otherwise
bool SortNode::simplify(ExecutionPlan* plan) {
  for (auto it = _elements.begin(); it != _elements.end(); /* no hoisting */) {
    auto variable = (*it).var;

    TRI_ASSERT(variable != nullptr);
    auto setter = _plan->getVarSetBy(variable->id);

    if (setter != nullptr) {
      if (setter->getType() == ExecutionNode::CALCULATION) {
        // variable introduced by a calculation
        auto expression =
            ExecutionNode::castTo<CalculationNode*>(setter)->expression();

        if (expression->isConstant()) {
          // constant expression, remove it!
          it = _elements.erase(it);
          continue;
        }
      }
    }

    ++it;
  }

  return _elements.empty();
}

/// @brief returns all sort information
SortInformation SortNode::getSortInformation() const {
  SortInformation result;
  std::string buffer;

  auto const& elms = elements();
  for (auto it = elms.begin(); it != elms.end(); ++it) {
    auto variable = (*it).var;
    TRI_ASSERT(variable != nullptr);
    auto setter = _plan->getVarSetBy(variable->id);

    if (setter == nullptr) {
      result.isValid = false;
      break;
    }

    if (setter->getType() == ExecutionNode::CALCULATION) {
      // variable introduced by a calculation
      auto expression =
          ExecutionNode::castTo<CalculationNode*>(setter)->expression();

      if (!expression->isDeterministic()) {
        result.isDeterministic = false;
      }

      if (!expression->isAttributeAccess() && !expression->isReference() &&
          !expression->isConstant()) {
        result.isComplex = true;
        break;
      }

      // fix buffer if it was in an invalid state of being moved-away
      buffer.clear();
      try {
        expression->stringify(buffer);
      } catch (...) {
        result.isValid = false;
        return result;
      }
      result.criteria.emplace_back(
          std::make_tuple(const_cast<ExecutionNode const*>(setter),
                          std::move(buffer), (*it).ascending));
    } else {
      // use variable only. note that we cannot use the variable's name as it is
      // not
      // necessarily unique in one query (yes, COLLECT, you are to blame!)
      result.criteria.emplace_back(
          std::make_tuple(const_cast<ExecutionNode const*>(setter),
                          std::to_string(variable->id), (*it).ascending));
    }
  }

  return result;
}

/// @brief clone ExecutionNode recursively
ExecutionNode* SortNode::clone(ExecutionPlan* plan,
                               bool withDependencies) const {
  return cloneHelper(std::make_unique<SortNode>(plan, _id, _elements, _stable),
                     withDependencies);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SortNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  std::vector<SortRegister> sortRegs;
  auto inputRegs = RegIdSet{};
  for (auto const& element : _elements) {
    auto it = getRegisterPlan()->varInfo.find(element.var->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    RegisterId id = it->second.registerId;
    sortRegs.emplace_back(id, element);
    inputRegs.emplace(id);
  }

  auto registerInfos = createRegisterInfos(std::move(inputRegs), {});
  auto executorInfos = SortExecutorInfos(
      registerInfos.numberOfInputRegisters(),
      registerInfos.numberOfOutputRegisters(), registerInfos.registersToClear(),
      std::move(sortRegs), _limit, engine.itemBlockManager(), engine.getQuery(),
      engine.getQuery()
          .vocbase()
          .server()
          .getFeature<TemporaryStorageFeature>(),
      &engine.getQuery().vpackOptions(), engine.getQuery().resourceMonitor(),
      engine.getQuery().queryOptions().spillOverThresholdNumRows,
      engine.getQuery().queryOptions().spillOverThresholdMemoryUsage, _stable);
  if (sorterType() == SorterType::kStandard) {
    return std::make_unique<ExecutionBlockImpl<SortExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  } else {
    return std::make_unique<ExecutionBlockImpl<ConstrainedSortExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
}

/// @brief estimateCost
CostEstimate SortNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  if (estimate.estimatedNrItems <= 3) {
    estimate.estimatedCost += estimate.estimatedNrItems;
  } else {
    estimate.estimatedCost +=
        estimate.estimatedNrItems *
        std::log2(static_cast<double>(estimate.estimatedNrItems));
  }
  return estimate;
}

AsyncPrefetchEligibility SortNode::canUseAsyncPrefetching() const noexcept {
  return AsyncPrefetchEligibility::kEnableForNode;
}

void SortNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  for (auto& variable : _elements) {
    variable.var = Variable::replace(variable.var, replacements);
  }
}

void SortNode::replaceAttributeAccess(ExecutionNode const* self,
                                      Variable const* searchVariable,
                                      std::span<std::string_view> attribute,
                                      Variable const* replaceVariable,
                                      size_t /*index*/) {
  for (auto& it : _elements) {
    it.replaceAttributeAccess(searchVariable, attribute, replaceVariable);
  }
}

/// @brief getVariablesUsedHere, modifying the set in-place
void SortNode::getVariablesUsedHere(VarSet& vars) const {
  for (auto& p : _elements) {
    vars.emplace(p.var);
  }
}

SortNode::SorterType SortNode::sorterType() const noexcept {
  return (!isStable() && _limit > 0) ? SorterType::kConstrainedHeap
                                     : SorterType::kStandard;
}

size_t SortNode::getMemoryUsedBytes() const { return sizeof(*this); }

std::string_view SortNode::sorterTypeName(SorterType type) noexcept {
  switch (type) {
    case SorterType::kConstrainedHeap:
      return "constrained-heap";
    case SorterType::kStandard:
    default:
      return "standard";
  }
}
