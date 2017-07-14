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

#include "ModificationNodes.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/VariableGenerator.h"

using namespace arangodb::aql;

static bool const Optional = true;

ModificationNode::ModificationNode(ExecutionPlan* plan,
                                   arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())),
      _options(base),
      _outVariableOld(
          Variable::varFromVPack(plan->getAst(), base, "outVariableOld", Optional)),
      _outVariableNew(
          Variable::varFromVPack(plan->getAst(), base, "outVariableNew", Optional)) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_collection != nullptr);
}

/// @brief toVelocyPack
void ModificationNode::toVelocyPackHelper(VPackBuilder& builder,
                                          bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(builder,
                                           verbose);  // call base class method
  // Now put info about vocbase and cid in there
  builder.add("database", VPackValue(_vocbase->name()));
  builder.add("collection", VPackValue(_collection->getName()));

  // add out variables
  if (_outVariableOld != nullptr) {
    builder.add(VPackValue("outVariableOld"));
    _outVariableOld->toVelocyPack(builder);
  }
  if (_outVariableNew != nullptr) {
    builder.add(VPackValue("outVariableNew"));
    _outVariableNew->toVelocyPack(builder);
  }
  builder.add(VPackValue("modificationFlags"));
  _options.toVelocyPack(builder);
}

/// @brief estimateCost
/// Note that all the modifying nodes use this estimateCost method which is
/// why we can make it final here.
double ModificationNode::estimateCost(size_t& nrItems) const {
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);
  if (_outVariableOld != nullptr || _outVariableNew != nullptr) {
    // node produces output
    nrItems = incoming;
  } else {
    // node produces no output
    nrItems = 0;
  }
  return depCost + incoming;
}

RemoveNode::RemoveNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")) {}

void RemoveNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ModificationNode::toVelocyPackHelper(nodes, verbose);
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* RemoveNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto outVariableOld = _outVariableOld;
  auto inVariable = _inVariable;

  if (withProperties) {
    if (_outVariableOld != nullptr) {
      outVariableOld =
          plan->getAst()->variables()->createVariable(outVariableOld);
    }
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }

  auto c = new RemoveNode(plan, _id, _vocbase, _collection, _options,
                          inVariable, outVariableOld);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

InsertNode::InsertNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")) {}

/// @brief toVelocyPack
void InsertNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ModificationNode::toVelocyPackHelper(nodes,
                                       verbose);  // call base class method

  // Now put info about vocbase and cid in there
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* InsertNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto outVariableNew = _outVariableNew;
  auto inVariable = _inVariable;

  if (withProperties) {
    if (_outVariableNew != nullptr) {
      outVariableNew =
          plan->getAst()->variables()->createVariable(outVariableNew);
    }
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }

  auto c = new InsertNode(plan, _id, _vocbase, _collection, _options,
                          inVariable, outVariableNew);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

UpdateNode::UpdateNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _inKeyVariable(
          Variable::varFromVPack(plan->getAst(), base, "inKeyVariable", Optional)) {}

/// @brief toVelocyPack
void UpdateNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ModificationNode::toVelocyPackHelper(nodes, verbose);
  
  nodes.add(VPackValue("inDocVariable"));
  _inDocVariable->toVelocyPack(nodes);

  // inKeyVariable might be empty
  if (_inKeyVariable != nullptr) {
    nodes.add(VPackValue("inKeyVariable"));
    _inKeyVariable->toVelocyPack(nodes);
  }

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* UpdateNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto outVariableOld = _outVariableOld;
  auto outVariableNew = _outVariableNew;
  auto inKeyVariable = _inKeyVariable;
  auto inDocVariable = _inDocVariable;

  if (withProperties) {
    if (_outVariableOld != nullptr) {
      outVariableOld =
          plan->getAst()->variables()->createVariable(outVariableOld);
    }
    if (_outVariableNew != nullptr) {
      outVariableNew =
          plan->getAst()->variables()->createVariable(outVariableNew);
    }
    if (inKeyVariable != nullptr) {
      inKeyVariable =
          plan->getAst()->variables()->createVariable(inKeyVariable);
    }
    inDocVariable = plan->getAst()->variables()->createVariable(inDocVariable);
  }

  auto c =
      new UpdateNode(plan, _id, _vocbase, _collection, _options, inDocVariable,
                     inKeyVariable, outVariableOld, outVariableNew);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

ReplaceNode::ReplaceNode(ExecutionPlan* plan,
                         arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _inKeyVariable(
          Variable::varFromVPack(plan->getAst(), base, "inKeyVariable", Optional)) {}

/// @brief toVelocyPack
void ReplaceNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ModificationNode::toVelocyPackHelper(nodes, verbose);

  nodes.add(VPackValue("inDocVariable"));
  _inDocVariable->toVelocyPack(nodes);

  // inKeyVariable might be empty
  if (_inKeyVariable != nullptr) {
    nodes.add(VPackValue("inKeyVariable"));
    _inKeyVariable->toVelocyPack(nodes);
  }

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* ReplaceNode::clone(ExecutionPlan* plan, bool withDependencies,
                                  bool withProperties) const {
  auto outVariableOld = _outVariableOld;
  auto outVariableNew = _outVariableNew;
  auto inKeyVariable = _inKeyVariable;
  auto inDocVariable = _inDocVariable;

  if (withProperties) {
    if (_outVariableOld != nullptr) {
      outVariableOld =
          plan->getAst()->variables()->createVariable(outVariableOld);
    }
    if (_outVariableNew != nullptr) {
      outVariableNew =
          plan->getAst()->variables()->createVariable(outVariableNew);
    }
    if (inKeyVariable != nullptr) {
      inKeyVariable =
          plan->getAst()->variables()->createVariable(inKeyVariable);
    }
    inDocVariable = plan->getAst()->variables()->createVariable(inDocVariable);
  }

  auto c =
      new ReplaceNode(plan, _id, _vocbase, _collection, _options, inDocVariable,
                      inKeyVariable, outVariableOld, outVariableNew);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

UpsertNode::UpsertNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _insertVariable(Variable::varFromVPack(plan->getAst(), base, "insertVariable")),
      _updateVariable(Variable::varFromVPack(plan->getAst(), base, "updateVariable")),
      _isReplace(base.get("isReplace").getBoolean()) {}

/// @brief toVelocyPack
void UpsertNode::toVelocyPackHelper(VPackBuilder& nodes,
                              bool verbose) const {
  ModificationNode::toVelocyPackHelper(nodes, verbose);

  nodes.add(VPackValue("inDocVariable"));
  _inDocVariable->toVelocyPack(nodes);
  nodes.add(VPackValue("insertVariable"));
  _insertVariable->toVelocyPack(nodes);
  nodes.add(VPackValue("updateVariable"));
  _updateVariable->toVelocyPack(nodes);
  nodes.add("isReplace", VPackValue(_isReplace));

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* UpsertNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto outVariableNew = _outVariableNew;
  auto inDocVariable = _inDocVariable;
  auto insertVariable = _insertVariable;
  auto updateVariable = _updateVariable;

  if (withProperties) {
    if (_outVariableNew != nullptr) {
      outVariableNew =
          plan->getAst()->variables()->createVariable(outVariableNew);
    }
    inDocVariable = plan->getAst()->variables()->createVariable(inDocVariable);
    insertVariable =
        plan->getAst()->variables()->createVariable(insertVariable);
    updateVariable =
        plan->getAst()->variables()->createVariable(updateVariable);
  }

  auto c = new UpsertNode(plan, _id, _vocbase, _collection, _options,
                          inDocVariable, insertVariable, updateVariable,
                          outVariableNew, _isReplace);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}
