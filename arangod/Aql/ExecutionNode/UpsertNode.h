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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ModificationOptions.h"

namespace arangodb::aql {
struct Collection;
class ExecutionBlock;
class ExecutionPlan;
struct Variable;

/// @brief class UpsertNode
class UpsertNode : public ModificationNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection name
 public:
  UpsertNode(ExecutionPlan* plan, ExecutionNodeId id,
             Collection const* collection, ModificationOptions const& options,
             Variable const* inDocVariable, Variable const* insertVariable,
             Variable const* updateVariable, Variable const* outVariableNew,
             bool isReplace, bool canReadOwnWrites);

  UpsertNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return UPSERT; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

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

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief input variable for the search document
  Variable const* _inDocVariable;

  /// @brief insert case expression
  Variable const* _insertVariable;

  /// @brief update case expression
  Variable const* _updateVariable;

  /// @brief whether to perform a REPLACE (or an UPDATE alternatively)
  bool _isReplace;

  bool _canReadOwnWrites;
};

}  // namespace arangodb::aql
