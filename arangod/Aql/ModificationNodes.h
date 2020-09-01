////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MODIFICATION_NODES_H
#define ARANGOD_AQL_MODIFICATION_NODES_H 1

#include "Aql/Ast.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ModificationOptions.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace aql {
struct Collection;
class ExecutionBlock;
class ExecutionPlan;

/// @brief abstract base class for modification operations
class ModificationNode : public ExecutionNode, public CollectionAccessingNode {
  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection and options
 protected:
  ModificationNode(ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
                   ModificationOptions const& options,
                   Variable const* outVariableOld, Variable const* outVariableNew)
      : ExecutionNode(plan, id),
        CollectionAccessingNode(collection),
        _options(options),
        _outVariableOld(outVariableOld),
        _outVariableNew(outVariableNew),
        _countStats(true),
        _producesResults(true) {}

  ModificationNode(ExecutionPlan*, arangodb::velocypack::Slice const& slice);

  /// @brief export to VelocyPack
  virtual void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                                  std::unordered_set<ExecutionNode const*>& seen) const override;

 public:
  /// @brief estimateCost
  /// Note that all the modifying nodes use this estimateCost method which is
  /// why we can make it final here.
  CostEstimate estimateCost() const override final;

  /// @brief data modification is non-deterministic
  bool isDeterministic() override final { return false; }

  /// @brief getOptions
  ModificationOptions const& getOptions() const { return _options; }

  /// @brief getOptions
  ModificationOptions& getOptions() { return _options; }

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    std::vector<Variable const*> v;
    if (_outVariableOld != nullptr) {
      v.emplace_back(_outVariableOld);
    }
    if (_outVariableNew != nullptr) {
      v.emplace_back(_outVariableNew);
    }
    return v;
  }

  /// @brief return the "$OLD" out variable
  Variable const* getOutVariableOld() const { return _outVariableOld; }

  /// @brief return the "$NEW" out variable
  Variable const* getOutVariableNew() const { return _outVariableNew; }

  /// @brief clear the "$OLD" out variable
  void clearOutVariableOld() { _outVariableOld = nullptr; }

  /// @brief clear the "$NEW" out variable
  void clearOutVariableNew() { _outVariableNew = nullptr; }

  /// @brief set the "$OLD" out variable
  void setOutVariableOld(Variable const* oldVar) { _outVariableOld = oldVar; }

  /// @brief set the "$NEW" out variable
  void setOutVariableNew(Variable const* newVar) { _outVariableNew = newVar; }

  /// @brief whether or not the node produces results
  /// this is normally turned on unless an optimizer rule
  /// explicitly turns this off as a performance optimization
  bool producesResults() const { return _producesResults; }

  /// @brief whether or not the node produces results
  void producesResults(bool value) { _producesResults = value; }

  /// @brief whether or not the node is a data modification node
  bool isModificationNode() const override { return true; }

  /// @brief whether this node contributes to statistics. Only disabled in
  /// SmartGraph case
  bool countStats() const { return _countStats; }

  /// @brief Disable that this node is contributing to statistics. Only disabled
  /// in SmartGraph case
  void disableStatistics() { _countStats = false; }

 protected:
  void cloneCommon(ModificationNode*) const;

 protected:
  /// @brief modification operation options
  ModificationOptions _options;

  /// @brief output variable ($OLD)
  Variable const* _outVariableOld;

  /// @brief output variable ($NEW)
  Variable const* _outVariableNew;

  /// @brief whether this node contributes to statistics. Only disabled in
  /// SmartGraph case
  bool _countStats;

  /// @brief whether this node will pass through results from block above
  bool _producesResults;
};

/// @brief class RemoveNode
class RemoveNode : public ModificationNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  RemoveNode(ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
             ModificationOptions const& options, Variable const* inVariable,
             Variable const* outVariableOld)
      : ModificationNode(plan, id, collection, options, outVariableOld, nullptr),
        _inVariable(inVariable) {
    TRI_ASSERT(_inVariable != nullptr);
  }

  RemoveNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return REMOVE; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final {
    vars.emplace(_inVariable);
  }

  void setInVariable(Variable const* var) { _inVariable = var; }

  Variable const* inVariable() const { return _inVariable; }

 private:
  /// @brief input variable
  Variable const* _inVariable;
};

/// @brief class InsertNode
class InsertNode : public ModificationNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  InsertNode(ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
             ModificationOptions const& options, Variable const* inVariable,
             Variable const* outVariableOld, Variable const* outVariableNew)
      : ModificationNode(plan, id, collection, options, outVariableOld, outVariableNew),
        _inVariable(inVariable) {
    TRI_ASSERT(_inVariable != nullptr);
    // _outVariable might be a nullptr
  }

  InsertNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return INSERT; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final {
    vars.emplace(_inVariable);
  }

  Variable const* inVariable() const { return _inVariable; }

  void setInVariable(Variable const* var) { _inVariable = var; }

 private:
  /// @brief input variable
  Variable const* _inVariable;
};

class UpdateReplaceNode : public ModificationNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  UpdateReplaceNode(ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
                    ModificationOptions const& options,
                    Variable const* inDocVariable, Variable const* inKeyVariable,
                    Variable const* outVariableOld, Variable const* outVariableNew)
      : ModificationNode(plan, id, collection, options, outVariableOld, outVariableNew),
        _inDocVariable(inDocVariable),
        _inKeyVariable(inKeyVariable) {
    TRI_ASSERT(_inDocVariable != nullptr);
    // _inKeyVariable might be a nullptr
  }

  UpdateReplaceNode(ExecutionPlan*, arangodb::velocypack::Slice const&);

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final {
    vars.emplace(_inDocVariable);

    if (_inKeyVariable != nullptr) {
      vars.emplace(_inKeyVariable);
    }
  }

  /// @brief set the input document variable
  void setInDocVariable(Variable const* var) { _inDocVariable = var; }

  Variable const* inKeyVariable() const { return _inKeyVariable; }

  Variable const* inDocVariable() const { return _inDocVariable; }

 protected:
  /// @brief input variable for documents
  Variable const* _inDocVariable;

  /// @brief input variable for keys
  Variable const* _inKeyVariable;
};

/// @brief class UpdateNode
class UpdateNode : public UpdateReplaceNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructor with a vocbase and a collection name
 public:
  UpdateNode(ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
             ModificationOptions const& options, Variable const* inDocVariable,
             Variable const* inKeyVariable, Variable const* outVariableOld,
             Variable const* outVariableNew)
      : UpdateReplaceNode(plan, id, collection, options, inDocVariable,
                          inKeyVariable, outVariableOld, outVariableNew) {}

  UpdateNode(ExecutionPlan*, arangodb::velocypack::Slice const&);

  /// @brief return the type of the node
  NodeType getType() const override final { return UPDATE; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;
};

/// @brief class ReplaceNode
class ReplaceNode : public UpdateReplaceNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructor with a vocbase and a collection name
 public:
  ReplaceNode(ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
              ModificationOptions const& options, Variable const* inDocVariable,
              Variable const* inKeyVariable, Variable const* outVariableOld,
              Variable const* outVariableNew)
      : UpdateReplaceNode(plan, id, collection, options, inDocVariable,
                          inKeyVariable, outVariableOld, outVariableNew) {}

  ReplaceNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return REPLACE; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;
};

/// @brief class UpsertNode
class UpsertNode : public ModificationNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructor with a vocbase and a collection name
 public:
  UpsertNode(ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
             ModificationOptions const& options, Variable const* inDocVariable,
             Variable const* insertVariable, Variable const* updateVariable,
             Variable const* outVariableNew, bool isReplace)
      : ModificationNode(plan, id, collection, options, nullptr, outVariableNew),
        _inDocVariable(inDocVariable),
        _insertVariable(insertVariable),
        _updateVariable(updateVariable),
        _isReplace(isReplace) {
    TRI_ASSERT(_inDocVariable != nullptr);
    TRI_ASSERT(_insertVariable != nullptr);
    TRI_ASSERT(_updateVariable != nullptr);

    TRI_ASSERT(_outVariableOld == nullptr);
  }

  UpsertNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return UPSERT; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final {
    vars.emplace(_inDocVariable);
    vars.emplace(_insertVariable);
    vars.emplace(_updateVariable);
  }

  Variable const* inDocVariable() const { return _inDocVariable; }

  void setInDocVariable(Variable const* var) { _inDocVariable = var; }

  Variable const* insertVariable() const { return _insertVariable; }

  void setInsertVariable(Variable const* var) { _insertVariable = var; }

  void setUpdateVariable(Variable const* var) { _updateVariable = var; }

  void setIsReplace(bool var) { _isReplace = var; }

 private:
  /// @brief input variable for the search document
  Variable const* _inDocVariable;

  /// @brief insert case expression
  Variable const* _insertVariable;

  /// @brief update case expression
  Variable const* _updateVariable;

  /// @brief whether to perform a REPLACE (or an UPDATE alternatively)
  bool _isReplace;
};

}  // namespace aql
}  // namespace arangodb

#endif
