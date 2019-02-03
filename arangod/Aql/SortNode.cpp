////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortRegister.h"
#include "Aql/SortExecutor.h"
#include "Aql/WalkerWorker.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/StringBuffer.h"

#ifdef USE_IRESEARCH
#include "IResearch/IResearchViewNode.h"
#include "IResearch/IResearchOrderFactory.h"

#if 0
namespace {

int compareIResearchScores(
    irs::sort::prepared const* comparer,
    arangodb::transaction::Methods*,
    arangodb::aql::AqlValue const& lhs,
    arangodb::aql::AqlValue const& rhs
) {
  arangodb::velocypack::ValueLength tmp;

  auto const* lhsScore = reinterpret_cast<irs::byte_type const*>(lhs.slice().getString(tmp));
  auto const* rhsScore = reinterpret_cast<irs::byte_type const*>(rhs.slice().getString(tmp));

  if (comparer->less(lhsScore, rhsScore)) {
    return -1;
  }

  if (comparer->less(rhsScore, lhsScore)) {
    return 1;
  }

  return 0;
}

int compareAqlValues(
    irs::sort::prepared const*,
    arangodb::transaction::Methods* trx,
    arangodb::aql::AqlValue const& lhs,
    arangodb::aql::AqlValue const& rhs) {
  return arangodb::aql::AqlValue::Compare(trx, lhs, rhs, true);
}
}
#endif
#endif

using namespace arangodb::basics;
using namespace arangodb::aql;

SortNode::SortNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
                   SortElementVector const& elements, bool stable)
    : ExecutionNode(plan, base),
      _reinsertInCluster(true),
      _elements(elements),
      _stable(stable) {}

/// @brief toVelocyPack, for SortNode
void SortNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           flags);  // call base class method

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

  // And close it:
  nodes.close();
}

class SortNodeFindMyExpressions : public WalkerWorker<ExecutionNode> {
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
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&
) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

#ifdef USE_IRESEARCH
  std::unordered_map<ExecutionNode const*, size_t> offsets;
  irs::sort::ptr comparer;
#endif

  std::vector<SortRegister> sortRegs;
  for(auto const& element : _elements){
    auto it = getRegisterPlan()->varInfo.find(element.var->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    RegisterId id = it->second.registerId;
#if 0 // #ifdef USE_IRESEARCH
    sortRegs.push_back(SortRegister{ id, element, &compareAqlValues });

    auto varId = element.var->id;
    auto const* setter = _plan->getVarSetBy(varId);
    if (setter && ExecutionNode::ENUMERATE_IRESEARCH_VIEW == setter->getType()) {
      // sort condition is covered by `IResearchViewNode`

      auto const* viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode const*>(setter);
      auto& offset = offsets[viewNode];
      auto* node = viewNode->sortCondition()[offset++].node;

      if (arangodb::iresearch::OrderFactory::comparer(&comparer, *node)) {
        auto& reg = sortRegs.back();
        reg.scorer = comparer->prepare(); // set score comparer
        reg.comparator = &compareIResearchScores; // set comparator
      }
    }
#else
    sortRegs.push_back(SortRegister{id, element});
#endif
  }

  SortExecutorInfos infos( std::move(sortRegs)
                         , getRegisterPlan()->nrRegs[previousNode->getDepth()]
                         , getRegisterPlan()->nrRegs[getDepth()]
                         , getRegsToClear()
                         , engine.getQuery()->trx()
                         , _stable);

  return std::make_unique<ExecutionBlockImpl<SortExecutor>>(&engine, this, std::move(infos));
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
