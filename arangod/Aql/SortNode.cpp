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
#include "Aql/ExecutionPlan.h"
#include "Aql/SortBlock.h"
#include "Aql/WalkerWorker.h"
#include "Basics/StringBuffer.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

SortNode::SortNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
                   SortElementVector const& elements, bool stable)
    : ExecutionNode(plan, base), _reinsertInCluster(true),  _elements(elements), _stable(stable){}

/// @brief toVelocyPack, for SortNode
void SortNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);  // call base class method

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
SortInformation SortNode::getSortInformation(
    ExecutionPlan* plan, arangodb::basics::StringBuffer* buffer) const {
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
          std::make_tuple(const_cast<ExecutionNode const*>(setter), std::string(buffer->c_str(), buffer->length()), (*it).ascending));
      buffer->reset();
    } else {
      // use variable only. note that we cannot use the variable's name as it is
      // not
      // necessarily unique in one query (yes, COLLECT, you are to blame!)
      result.criteria.emplace_back(
          std::make_tuple(const_cast<ExecutionNode const*>(setter), std::to_string(variable->id), (*it).ascending));
    }
  }

  return result;
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> SortNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&
) const {
  return std::make_unique<SortBlock>(&engine, this);
}

/// @brief estimateCost
CostEstimate SortNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  if (estimate.estimatedNrItems <= 3) {
    estimate.estimatedCost += estimate.estimatedNrItems;
  } else {
    estimate.estimatedCost += estimate.estimatedNrItems * std::log2(static_cast<double>(estimate.estimatedNrItems));
  }
  return estimate;
}

