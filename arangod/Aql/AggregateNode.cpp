////////////////////////////////////////////////////////////////////////////////
/// @brief AggregateNode
///
/// @file 
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
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "AggregateNode.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/WalkerWorker.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                          methods of AggregateNode
// -----------------------------------------------------------------------------

AggregateNode::AggregateNode (ExecutionPlan* plan,
                              triagens::basics::Json const& base,
                              Variable const* expressionVariable,
                              Variable const* outVariable,
                              std::vector<Variable const*> const& keepVariables,
                              std::unordered_map<VariableId, std::string const> const& variableMap,
                              std::vector<std::pair<Variable const*, Variable const*>> const& aggregateVariables,
                              bool count,
                              bool isDistinctCommand)
  : ExecutionNode(plan, base),
    _options(base),
    _aggregateVariables(aggregateVariables), 
    _expressionVariable(expressionVariable),
    _outVariable(outVariable),
    _keepVariables(keepVariables),
    _variableMap(variableMap),
    _count(count),
    _isDistinctCommand(isDistinctCommand),
    _specialized(false) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for AggregateNode
////////////////////////////////////////////////////////////////////////////////

void AggregateNode::toJsonHelper (triagens::basics::Json& nodes,
                                  TRI_memory_zone_t* zone,
                                  bool verbose) const {

  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  triagens::basics::Json values(triagens::basics::Json::Array, _aggregateVariables.size());

  for (auto it = _aggregateVariables.begin(); it != _aggregateVariables.end(); ++it) {
    triagens::basics::Json variable(triagens::basics::Json::Object);
    variable("outVariable", (*it).first->toJson())
            ("inVariable", (*it).second->toJson());
    values(variable);
  }
  json("aggregates", values);

  // expression variable might be empty
  if (_expressionVariable != nullptr) {
    json("expressionVariable", _expressionVariable->toJson());
  }

  // output variable might be empty
  if (_outVariable != nullptr) {
    json("outVariable", _outVariable->toJson());
  }

  if (! _keepVariables.empty()) {
    triagens::basics::Json values(triagens::basics::Json::Array, _keepVariables.size());
    for (auto it = _keepVariables.begin(); it != _keepVariables.end(); ++it) {
      triagens::basics::Json variable(triagens::basics::Json::Object);
      variable("variable", (*it)->toJson());
      values(variable);
    }
    json("keepVariables", values);
  }

  json("count", triagens::basics::Json(_count));
  json("isDistinctCommand", triagens::basics::Json(_isDistinctCommand));
  json("specialized", triagens::basics::Json(_specialized));
  
  _options.toJson(json, zone);

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* AggregateNode::clone (ExecutionPlan* plan,
                                     bool withDependencies,
                                     bool withProperties) const {
  auto outVariable = _outVariable;
  auto expressionVariable = _expressionVariable;
  auto aggregateVariables = _aggregateVariables;

  if (withProperties) {
    if (expressionVariable != nullptr) {
      expressionVariable = plan->getAst()->variables()->createVariable(expressionVariable);
    }

    if (outVariable != nullptr) {
      outVariable = plan->getAst()->variables()->createVariable(outVariable);
    }

    // need to re-create all variables
    aggregateVariables.clear();

    for (auto& it : _aggregateVariables) {
      auto out = plan->getAst()->variables()->createVariable(it.first);
      auto in  = plan->getAst()->variables()->createVariable(it.second);
      aggregateVariables.emplace_back(std::make_pair(out, in));
    }
  }

  auto c = new AggregateNode(
    plan, 
    _id,
    _options, 
    aggregateVariables, 
    expressionVariable, 
    outVariable, 
    _keepVariables, 
    _variableMap,
    _count,
    _isDistinctCommand
  );

  // specialize the cloned node
  if (isSpecialized()) {
    c->specialized();
  }

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

struct UserVarFinder final : public WalkerWorker<ExecutionNode> {
  UserVarFinder (int mindepth) 
    : mindepth(mindepth), depth(-1) { 
  }

  ~UserVarFinder () {
  }

  std::vector<Variable const*> userVars;
  int mindepth;   // minimal depth to consider
  int depth;

  bool enterSubquery (ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  void after (ExecutionNode* en) override final {
    if (en->getType() == ExecutionNode::SINGLETON) {
      depth = 0;
    }
    else if (en->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
             en->getType() == ExecutionNode::INDEX ||
             en->getType() == ExecutionNode::ENUMERATE_LIST ||
             en->getType() == ExecutionNode::AGGREGATE) {
      depth += 1;
    }
    // Now depth is set correct for this node.
    if (depth >= mindepth) {
      auto const& vars = en->getVariablesSetHere();
      for (auto const& v : vars) {
        if (v->isUserDefined()) {
          userVars.emplace_back(v);
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

std::vector<Variable const*> AggregateNode::getVariablesUsedHere () const {
  std::unordered_set<Variable const*> v;
  // actual work is done by that method
  getVariablesUsedHere(v);

  // copy result into vector
  std::vector<Variable const*> vv;
  vv.reserve(v.size());
  for (auto const& x : v) {
    vv.emplace_back(x);
  }
  return vv;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

void AggregateNode::getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const {
  for (auto const& p : _aggregateVariables) {
    vars.emplace(p.second);
  }

  if (_expressionVariable != nullptr) {
    vars.emplace(_expressionVariable);
  }

  if (_outVariable != nullptr && ! _count) {
    if (_keepVariables.empty()) {
      // Here we have to find all user defined variables in this query
      // amongst our dependencies:
      UserVarFinder finder(1);
      auto myselfAsNonConst = const_cast<AggregateNode*>(this);
      myselfAsNonConst->walk(&finder);
      if (finder.depth == 1) {
        // we are top level, let's run again with mindepth = 0
        finder.userVars.clear();
        finder.mindepth = 0;
        finder.depth = -1;
        finder.reset();
        myselfAsNonConst->walk(&finder);
      }
      for (auto& x : finder.userVars) {
        vars.emplace(x);
      }
    }
    else {
      for (auto& x : _keepVariables) {
        vars.emplace(x);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////
        
double AggregateNode::estimateCost (size_t& nrItems) const {
  double depCost = _dependencies.at(0)->getCost(nrItems);
  
  // As in the FilterNode case, we are pessimistic here by not reducing the
  // nrItems much, since the worst case for COLLECT is to return as many items
  // as there are input items. In any case, we have to look at all incoming
  // items, and in particular in the COLLECT ... INTO ... case, we have
  // to actually hand on all data anyway, albeit not as separate items.
  // Nevertheless, the optimizer does not do much with AggregateNodes
  // and thus this potential overestimation does not really matter.


  if (_count && _aggregateVariables.empty()) {
    // we are known to only produce a single output row
    nrItems = 1;
  }
  else {
    // we do not know how many rows the COLLECT with produce...
    // the worst case is that there will be as many output rows as input rows
    if (nrItems >= 10) {
      // we assume that the collect will reduce the number of results at least somewhat
      nrItems = static_cast<size_t>(nrItems * 0.80);
    }
  }

  return depCost + nrItems;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

