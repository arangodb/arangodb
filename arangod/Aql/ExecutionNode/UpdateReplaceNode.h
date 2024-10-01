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

class UpdateReplaceNode : public ModificationNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  UpdateReplaceNode(ExecutionPlan* plan, ExecutionNodeId id,
                    Collection const* collection,
                    ModificationOptions const& options,
                    Variable const* inDocVariable,
                    Variable const* inKeyVariable,
                    Variable const* outVariableOld,
                    Variable const* outVariableNew);

  UpdateReplaceNode(ExecutionPlan*, arangodb::velocypack::Slice const&);

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief set the input key variable
  void setInKeyVariable(Variable const* var) { _inKeyVariable = var; }

  /// @brief set the input document variable
  void setInDocVariable(Variable const* var) { _inDocVariable = var; }

  Variable const* inKeyVariable() const { return _inKeyVariable; }

  Variable const* inDocVariable() const { return _inDocVariable; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override;

  /// @brief input variable for documents
  Variable const* _inDocVariable;

  /// @brief input variable for keys
  Variable const* _inKeyVariable;
};

}  // namespace arangodb::aql
