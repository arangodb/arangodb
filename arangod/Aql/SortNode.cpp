////////////////////////////////////////////////////////////////////////////////
/// @brief SortNode
///
/// @file arangod/Aql/ExecutionNode.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SortNode.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/WalkerWorker.h"
#include "Basics/StringBuffer.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                               methods of SortNode
// -----------------------------------------------------------------------------

SortNode::SortNode (ExecutionPlan* plan,
                    triagens::basics::Json const& base,
                    SortElementVector const& elements,
                    bool stable)
  : ExecutionNode(plan, base),
    _elements(elements),
    _stable(stable) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SortNode
////////////////////////////////////////////////////////////////////////////////

void SortNode::toJsonHelper (triagens::basics::Json& nodes,
                             TRI_memory_zone_t* zone,
                             bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }
  triagens::basics::Json values(triagens::basics::Json::Array, _elements.size());
  for (auto it = _elements.begin(); it != _elements.end(); ++it) {
    triagens::basics::Json element(triagens::basics::Json::Object);
    element("inVariable", (*it).first->toJson())
      ("ascending", triagens::basics::Json((*it).second));
    values(element);
  }
  json("elements", values);
  json("stable", triagens::basics::Json(_stable));

  // And add it:
  nodes(json);
}

class SortNodeFindMyExpressions : public WalkerWorker<ExecutionNode> {

public:
  size_t _foundCalcNodes;
  SortElementVector _elms;
  std::vector<std::pair<ExecutionNode*, bool>> _myVars;

  SortNodeFindMyExpressions(SortNode* me)
    : _foundCalcNodes(0),
      _elms(me->getElements()) {
    _myVars.resize(_elms.size());
  }

  bool before (ExecutionNode* en) override final {
    auto vars = en->getVariablesSetHere();
    for (auto const& v : vars) {
      for (size_t n = 0; n < _elms.size(); n++) {
        if (_elms[n].first->id == v->id) {
          _myVars[n] = std::make_pair(en, _elms[n].second);
          _foundCalcNodes ++;
          break;
        }
      }
    }
    return _foundCalcNodes >= _elms.size();
  }
};

std::vector<std::pair<ExecutionNode*, bool>> SortNode::getCalcNodePairs () {
  SortNodeFindMyExpressions findExp(this);
  _dependencies[0]->walk(&findExp);
  if (findExp._foundCalcNodes < _elements.size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "SortNode wasn't able to locate all its CalculationNodes");
  }
  return findExp._myVars;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief simplifies the expressions of the sort node
/// this will sort expressions if they are constant
/// the method will return true if all sort expressions were removed after
/// simplification, and false otherwise
////////////////////////////////////////////////////////////////////////////////

bool SortNode::simplify (ExecutionPlan* plan) {
  for (auto it = _elements.begin(); it != _elements.end(); /* no hoisting */) {
    auto variable = (*it).first;
    
    TRI_ASSERT(variable != nullptr);
    auto setter = _plan->getVarSetBy(variable->id);
    
    if (setter != nullptr) {
      if (setter->getType() == ExecutionNode::CALCULATION) {
        // variable introduced by a calculation
        auto expression = static_cast<CalculationNode*>(setter)->expression();

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

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all sort information 
////////////////////////////////////////////////////////////////////////////////

SortInformation SortNode::getSortInformation (ExecutionPlan* plan,
                                              triagens::basics::StringBuffer* buffer) const {
  SortInformation result;

  auto elements = getElements();
  for (auto it = elements.begin(); it != elements.end(); ++it) {
    auto variable = (*it).first;
    TRI_ASSERT(variable != nullptr);
    auto setter = _plan->getVarSetBy(variable->id);
     
    if (setter == nullptr) {
      result.isValid = false;
      break;
    }
      
    if (! result.canThrow && setter->canThrow()) {
      result.canThrow = true;
    }

    if (setter->getType() == ExecutionNode::CALCULATION) {
      // variable introduced by a calculation
      auto expression = static_cast<CalculationNode*>(setter)->expression();

      if (! expression->isDeterministic()) {
        result.isDeterministic = false;
      }

      if (! expression->isAttributeAccess() &&
          ! expression->isReference() &&
          ! expression->isConstant()) {
        result.isComplex = true;
        break;
      }

      expression->stringify(buffer);
      result.criteria.emplace_back(std::make_tuple(setter, buffer->c_str(), (*it).second));
      buffer->reset();
    }
    else {
      // use variable only. note that we cannot use the variable's name as it is not
      // necessarily unique in one query (yes, COLLECT, you are to blame!)
      result.criteria.emplace_back(std::make_tuple(setter, std::to_string(variable->id), (*it).second));
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////
        
double SortNode::estimateCost (size_t& nrItems) const {
  double depCost = _dependencies.at(0)->getCost(nrItems);
  if (nrItems <= 3.0) {
    return depCost + nrItems;
  }
  return depCost + nrItems * log(nrItems);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

