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
#include "Aql/ModificationBlocks.h"
#include "Aql/Query.h"
#include "Aql/VariableGenerator.h"

using namespace arangodb::aql;

static bool const Optional = true;

ModificationNode::ModificationNode(ExecutionPlan* plan,
                                   arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      CollectionAccessingNode(plan, base),
      _options(base),
      _outVariableOld(
          Variable::varFromVPack(plan->getAst(), base, "outVariableOld", Optional)),
      _outVariableNew(
          Variable::varFromVPack(plan->getAst(), base, "outVariableNew", Optional)),
      _countStats(base.get("countStats").getBool()) {
}

/// @brief toVelocyPack
void ModificationNode::toVelocyPackHelper(VPackBuilder& builder,
                                          unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(builder, flags);
  
  // add collection information
  CollectionAccessingNode::toVelocyPack(builder);

  // Now put info about vocbase and cid in there
  builder.add("countStats", VPackValue(_countStats));

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
CostEstimate ModificationNode::estimateCost(CostEstimate const& parent) const {
  CostEstimate estimate = CostEstimate::empty() + _dependencies.at(0)->getCost(parent);
  estimate.estimatedCost += estimate.estimatedNrItems;
  if (_outVariableOld == nullptr && _outVariableNew == nullptr) {
    // node produces no output
    estimate.estimatedNrItems = 0;
  }
  return estimate;
}

RemoveNode::RemoveNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")) {}

void RemoveNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ModificationNode::toVelocyPackHelper(nodes, flags);
  ModificationNode::toVelocyPackHelperPrimaryIndex(nodes);
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> RemoveNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&
) const {
  return std::make_unique<RemoveBlock>(&engine, this);
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

  auto c = std::make_unique<RemoveNode>(plan, _id, _collection, _options,
                          inVariable, outVariableOld);
  if (!_countStats) {
    c->disableStatistics();
  }

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

InsertNode::InsertNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inVariable(Variable::varFromVPack(plan->getAst(), base, "inVariable")) {}

/// @brief toVelocyPack
void InsertNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ModificationNode::toVelocyPackHelper(nodes, flags); // call base class method

  // Now put info about vocbase and cid in there
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> InsertNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&
) const {
  return std::make_unique<InsertBlock>(&engine, this);
}

/// @brief clone ExecutionNode recursively
ExecutionNode* InsertNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto outVariableOld = _outVariableOld;
  auto outVariableNew = _outVariableNew;
  auto inVariable = _inVariable;

  if (withProperties) {
    if (_outVariableNew != nullptr) {
      outVariableNew =
          plan->getAst()->variables()->createVariable(outVariableNew);
    }
    if (_outVariableOld != nullptr) {
      outVariableOld =
          plan->getAst()->variables()->createVariable(outVariableOld);
    }
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }

  auto c = std::make_unique<InsertNode>(plan, _id, _collection, _options,
                          inVariable, outVariableOld, outVariableNew);
  if (!_countStats) {
    c->disableStatistics();
  }

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

UpdateNode::UpdateNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _inKeyVariable(
          Variable::varFromVPack(plan->getAst(), base, "inKeyVariable", Optional)) {}

/// @brief toVelocyPack
void UpdateNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ModificationNode::toVelocyPackHelper(nodes, flags);
  ModificationNode::toVelocyPackHelperPrimaryIndex(nodes);

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

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> UpdateNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&
) const {
  return std::make_unique<UpdateBlock>(&engine, this);
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

  auto c = std::make_unique<UpdateNode>(plan, _id, _collection, _options, inDocVariable,
                     inKeyVariable, outVariableOld, outVariableNew);
  if (!_countStats) {
    c->disableStatistics();
  }

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

ReplaceNode::ReplaceNode(ExecutionPlan* plan,
                         arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _inKeyVariable(
          Variable::varFromVPack(plan->getAst(), base, "inKeyVariable", Optional)) {}

/// @brief toVelocyPack
void ReplaceNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ModificationNode::toVelocyPackHelper(nodes, flags);
  ModificationNode::toVelocyPackHelperPrimaryIndex(nodes);

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

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ReplaceNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&
) const {
  return std::make_unique<ReplaceBlock>(&engine, this);
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

  auto c = std::make_unique<ReplaceNode>(plan, _id, _collection, _options, inDocVariable,
                      inKeyVariable, outVariableOld, outVariableNew);
  if (!_countStats) {
    c->disableStatistics();
  }

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

UpsertNode::UpsertNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ModificationNode(plan, base),
      _inDocVariable(Variable::varFromVPack(plan->getAst(), base, "inDocVariable")),
      _insertVariable(Variable::varFromVPack(plan->getAst(), base, "insertVariable")),
      _updateVariable(Variable::varFromVPack(plan->getAst(), base, "updateVariable")),
      _isReplace(base.get("isReplace").getBoolean()) {}

/// @brief toVelocyPack
void UpsertNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  ModificationNode::toVelocyPackHelper(nodes, flags);
  ModificationNode::toVelocyPackHelperPrimaryIndex(nodes);

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

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> UpsertNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&
) const {
  return std::make_unique<UpsertBlock>(&engine, this);
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

  auto c = std::make_unique<UpsertNode>(plan, _id, _collection, _options,
                          inDocVariable, insertVariable, updateVariable,
                          outVariableNew, _isReplace);
  if (!_countStats) {
    c->disableStatistics();
  }

  return cloneHelper(std::move(c), withDependencies, withProperties);
}
