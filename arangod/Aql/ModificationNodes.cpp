////////////////////////////////////////////////////////////////////////////////
/// @brief Modification nodes
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

#include "ModificationNodes.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

const static bool Optional = true;

// -----------------------------------------------------------------------------
// --SECTION--                                                  ModificationNode
// -----------------------------------------------------------------------------

ModificationNode::ModificationNode (ExecutionPlan* plan,
                                    triagens::basics::Json const& base)
  : ExecutionNode(plan, base), 
    _vocbase(plan->getAst()->query()->vocbase()),
    _collection(plan->getAst()->query()->collections()->get(JsonHelper::checkAndGetStringValue(base.json(), "collection"))),
    _options(base),
    _outVariableOld(varFromJson(plan->getAst(), base, "outVariableOld", Optional)),
    _outVariableNew(varFromJson(plan->getAst(), base, "outVariableNew", Optional)) {

  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_collection != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void ModificationNode::toJsonHelper (triagens::basics::Json& json,
                                     TRI_memory_zone_t* zone,
                                     bool) const {

  // Now put info about vocbase and cid in there
  json("database", triagens::basics::Json(_vocbase->_name))
      ("collection", triagens::basics::Json(_collection->getName()));
  
  // add out variables
  if (_outVariableOld != nullptr) {
    json("outVariableOld", _outVariableOld->toJson());
  }
  if (_outVariableNew != nullptr) {
    json("outVariableNew", _outVariableNew->toJson());
  }

  _options.toJson(json, zone);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
/// Note that all the modifying nodes use this estimateCost method which is
/// why we can make it final here.
////////////////////////////////////////////////////////////////////////////////
        
double ModificationNode::estimateCost (size_t& nrItems) const {
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);
  if (_outVariableOld != nullptr || 
      _outVariableNew != nullptr) {
    // node produces output
    nrItems = incoming;
  }
  else {
    // node produces no output
    nrItems = 0;
  }
  return depCost + incoming;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of RemoveNode
// -----------------------------------------------------------------------------

RemoveNode::RemoveNode (ExecutionPlan* plan, 
                        triagens::basics::Json const& base)
  : ModificationNode(plan, base),
    _inVariable(varFromJson(plan->getAst(), base, "inVariable")) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void RemoveNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone,
                               bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("inVariable", _inVariable->toJson());
  
  ModificationNode::toJsonHelper(json, zone, verbose);

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* RemoveNode::clone (ExecutionPlan* plan,
                                  bool withDependencies,
                                  bool withProperties) const {
  auto outVariableOld = _outVariableOld;
  auto inVariable = _inVariable;

  if (withProperties) {
    if (_outVariableOld != nullptr) {
      outVariableOld = plan->getAst()->variables()->createVariable(outVariableOld);
    }
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }

  auto c = new RemoveNode(plan, _id, _vocbase, _collection, _options, inVariable, outVariableOld);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of InsertNode
// -----------------------------------------------------------------------------

InsertNode::InsertNode (ExecutionPlan* plan, 
                        triagens::basics::Json const& base)
  : ModificationNode(plan, base),
    _inVariable(varFromJson(plan->getAst(), base, "inVariable")) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void InsertNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone,
                               bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("inVariable", _inVariable->toJson()); 

  ModificationNode::toJsonHelper(json, zone, verbose);

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* InsertNode::clone (ExecutionPlan* plan,
                                  bool withDependencies,
                                  bool withProperties) const {
  auto outVariableNew = _outVariableNew;
  auto inVariable = _inVariable;

  if (withProperties) {
    if (_outVariableNew != nullptr) {
      outVariableNew = plan->getAst()->variables()->createVariable(outVariableNew);
    }
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }

  auto c = new InsertNode(plan, _id, _vocbase, _collection, _options, inVariable, outVariableNew);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of UpdateNode
// -----------------------------------------------------------------------------

UpdateNode::UpdateNode (ExecutionPlan* plan, 
                        triagens::basics::Json const& base)
  : ModificationNode(plan, base),
    _inDocVariable(varFromJson(plan->getAst(), base, "inDocVariable")),
    _inKeyVariable(varFromJson(plan->getAst(), base, "inKeyVariable", Optional)) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void UpdateNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone,
                               bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("inDocVariable", _inDocVariable->toJson());
  
  ModificationNode::toJsonHelper(json, zone, verbose);

  // inKeyVariable might be empty
  if (_inKeyVariable != nullptr) {
    json("inKeyVariable", _inKeyVariable->toJson());
  }
  
  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* UpdateNode::clone (ExecutionPlan* plan,
                                  bool withDependencies,
                                  bool withProperties) const {
  auto outVariableOld = _outVariableOld;
  auto outVariableNew = _outVariableNew;
  auto inKeyVariable = _inKeyVariable;
  auto inDocVariable = _inDocVariable;

  if (withProperties) {
    if (_outVariableOld != nullptr) {
      outVariableOld = plan->getAst()->variables()->createVariable(outVariableOld);
    }
    if (_outVariableNew != nullptr) {
      outVariableNew = plan->getAst()->variables()->createVariable(outVariableNew);
    }
    if (inKeyVariable != nullptr) {
      inKeyVariable = plan->getAst()->variables()->createVariable(inKeyVariable);
    }
    inDocVariable = plan->getAst()->variables()->createVariable(inDocVariable);
  }

  auto c = new UpdateNode(plan, _id, _vocbase, _collection, _options, inDocVariable, inKeyVariable, outVariableOld, outVariableNew);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            methods of ReplaceNode
// -----------------------------------------------------------------------------

ReplaceNode::ReplaceNode (ExecutionPlan* plan, 
                          triagens::basics::Json const& base)
  : ModificationNode(plan, base),
    _inDocVariable(varFromJson(plan->getAst(), base, "inDocVariable")),
    _inKeyVariable(varFromJson(plan->getAst(), base, "inKeyVariable", Optional)) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void ReplaceNode::toJsonHelper (triagens::basics::Json& nodes,
                                TRI_memory_zone_t* zone,
                                bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("inDocVariable", _inDocVariable->toJson());

  ModificationNode::toJsonHelper(json, zone, verbose);

  // inKeyVariable might be empty
  if (_inKeyVariable != nullptr) {
    json("inKeyVariable", _inKeyVariable->toJson());
  }
  
  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* ReplaceNode::clone (ExecutionPlan* plan,
                                   bool withDependencies,
                                   bool withProperties) const {
  auto outVariableOld = _outVariableOld;
  auto outVariableNew = _outVariableNew;
  auto inKeyVariable = _inKeyVariable;
  auto inDocVariable = _inDocVariable;

  if (withProperties) {
    if (_outVariableOld != nullptr) {
      outVariableOld = plan->getAst()->variables()->createVariable(outVariableOld);
    }
    if (_outVariableNew != nullptr) {
      outVariableNew = plan->getAst()->variables()->createVariable(outVariableNew);
    }
    if (inKeyVariable != nullptr) {
      inKeyVariable = plan->getAst()->variables()->createVariable(inKeyVariable);
    }
    inDocVariable = plan->getAst()->variables()->createVariable(inDocVariable);
  }

  auto c = new ReplaceNode(plan, _id, _vocbase, _collection, 
                           _options, inDocVariable, inKeyVariable,
                           outVariableOld, outVariableNew);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of UpsertNode
// -----------------------------------------------------------------------------

UpsertNode::UpsertNode (ExecutionPlan* plan, 
                        triagens::basics::Json const& base)
  : ModificationNode(plan, base),
    _inDocVariable(varFromJson(plan->getAst(), base, "inDocVariable")),
    _insertVariable(varFromJson(plan->getAst(), base, "insertVariable")),
    _updateVariable(varFromJson(plan->getAst(), base, "updateVariable")),
    _isReplace(JsonHelper::checkAndGetBooleanValue(base.json(), "isReplace")) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void UpsertNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone,
                               bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  ModificationNode::toJsonHelper(json, zone, verbose);

  json("inDocVariable", _inDocVariable->toJson());
  json("insertVariable", _insertVariable->toJson());
  json("updateVariable", _updateVariable->toJson());
  json("isReplace", triagens::basics::Json(_isReplace));
  
  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* UpsertNode::clone (ExecutionPlan* plan,
                                  bool withDependencies,
                                  bool withProperties) const {
  auto outVariableNew = _outVariableNew;
  auto inDocVariable = _inDocVariable;
  auto insertVariable = _insertVariable;
  auto updateVariable = _updateVariable;

  if (withProperties) {
    if (_outVariableNew != nullptr) {
      outVariableNew = plan->getAst()->variables()->createVariable(outVariableNew);
    }
    inDocVariable = plan->getAst()->variables()->createVariable(inDocVariable);
    insertVariable = plan->getAst()->variables()->createVariable(insertVariable);
    updateVariable = plan->getAst()->variables()->createVariable(updateVariable);
  }

  auto c = new UpsertNode(
    plan, 
    _id, 
    _vocbase, 
    _collection, 
    _options, 
    inDocVariable, 
    insertVariable, 
    updateVariable, 
    outVariableNew, 
    _isReplace
  );

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

