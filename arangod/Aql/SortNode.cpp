////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "SortNode.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/ConstrainedSortExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/WalkerWorker.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

namespace {
std::string const ConstrainedHeap = "constrained-heap";
std::string const Standard = "standard";
}  // namespace

using namespace arangodb::basics;
using namespace arangodb::aql;

std::string const& SortNode::sorterTypeName(SorterType type) {
  switch (type) {
    case SorterType::Standard:
      return ::Standard;
    case SorterType::ConstrainedHeap:
      return ::ConstrainedHeap;
    default:
      return ::Standard;
  }
}

SortNode::SortNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
                   SortElementVector const& elements, bool stable)
    : ExecutionNode(plan, base),
      _reinsertInCluster(true),
      _elements(elements),
      _stable(stable),
      _limit(VelocyPackHelper::getNumericValue<size_t>(base, "limit", 0)) {}

/// @brief toVelocyPack, for SortNode
void SortNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                  std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

  nodes.add(VPackValue("elements"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _elements) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("inVariable"));
      it.var->toVelocyPack(nodes);
      nodes.add("ascending", VPackValue(it.ascending));
      if (!it.attributePath.empty()) {
        nodes.add(VPackValue("path"));
        VPackArrayBuilder arr(&nodes);
        for (auto const& a : it.attributePath) {
          nodes.add(VPackValue(a));
        }
      }
    }
  }
  nodes.add("stable", VPackValue(_stable));
  nodes.add("limit", VPackValue(_limit));
  nodes.add("strategy", VPackValue(sorterTypeName(sorterType())));

  // And close it:
  nodes.close();
}

class SortNodeFindMyExpressions
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  size_t _foundCalcNodes;
  SortElementVector _elms;
  std::vector<std::pair<ExecutionNode*, bool>> _myVars;

  explicit SortNodeFindMyExpressions(SortNode* me)
      : _foundCalcNodes(0), _elms(me->elements()) {
    _myVars.resize(_elms.size());
  }

  bool before(ExecutionNode* en) override final {
    auto vars = en->getVariablesSetHere();
    for (auto const& v : vars) {
      for (size_t n = 0; n < _elms.size(); n++) {
        if (_elms[n].var->id == v->id) {
          _myVars[n] = std::make_pair(en, _elms[n].ascending);
          _foundCalcNodes++;
          break;
        }
      }
    }
    return _foundCalcNodes >= _elms.size();
  }
};

std::vector<std::pair<ExecutionNode*, bool>> SortNode::getCalcNodePairs() {
  SortNodeFindMyExpressions findExp(this);
  _dependencies[0]->walk(findExp);
  if (findExp._foundCalcNodes < _elements.size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "SortNode wasn't able to locate all its CalculationNodes");
  }
  return findExp._myVars;
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
        auto expression = ExecutionNode::castTo<CalculationNode*>(setter)->expression();

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

void SortNode::removeConditions(size_t count) {
  TRI_ASSERT(_elements.size() > count);
  TRI_ASSERT(count > 0);
  _elements.erase(_elements.begin(), _elements.begin() + count);
}

/// @brief returns all sort information
SortInformation SortNode::getSortInformation(ExecutionPlan* plan,
                                             arangodb::basics::StringBuffer* buffer) const {
  SortInformation result;

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
      auto expression = ExecutionNode::castTo<CalculationNode*>(setter)->expression();

      if (!expression->isDeterministic()) {
        result.isDeterministic = false;
      }

      if (!expression->isAttributeAccess() && !expression->isReference() &&
          !expression->isConstant()) {
        result.isComplex = true;
        break;
      }

      try {
        expression->stringify(buffer);
      } catch (...) {
        result.isValid = false;
        return result;
      }
      result.criteria.emplace_back(
          std::make_tuple(const_cast<ExecutionNode const*>(setter),
                          std::string(buffer->c_str(), buffer->length()), (*it).ascending));
      buffer->reset();
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

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SortNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
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
  auto executorInfos =
      SortExecutorInfos(registerInfos.numberOfInputRegisters(),
                        registerInfos.numberOfOutputRegisters(),
                        registerInfos.registersToClear(), std::move(sortRegs),
                        _limit, engine.itemBlockManager(),
                        &engine.getQuery().vpackOptions(),
                        engine.getQuery().resourceMonitor(),
                        _stable);
  if (sorterType() == SorterType::Standard) {
    return std::make_unique<ExecutionBlockImpl<SortExecutor>>(&engine, this,
                                                              std::move(registerInfos),
                                                              std::move(executorInfos));
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
    estimate.estimatedCost += estimate.estimatedNrItems *
                              std::log2(static_cast<double>(estimate.estimatedNrItems));
  }
  return estimate;
}

SortNode::SorterType SortNode::sorterType() const {
  return (!isStable() && _limit > 0) ? SorterType::ConstrainedHeap : SorterType::Standard;
}
