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

#include "Aql/ExecutionNode/UpdateReplaceNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ModificationOptions.h"

namespace arangodb::aql {
struct Collection;
class ExecutionBlock;
class ExecutionPlan;
struct Variable;

/// @brief class UpdateNode
class UpdateNode : public UpdateReplaceNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection name
 public:
  UpdateNode(ExecutionPlan* plan, ExecutionNodeId id,
             Collection const* collection, ModificationOptions const& options,
             Variable const* inDocVariable, Variable const* inKeyVariable,
             Variable const* outVariableOld, Variable const* outVariableNew)
      : UpdateReplaceNode(plan, id, collection, options, inDocVariable,
                          inKeyVariable, outVariableOld, outVariableNew) {}

  UpdateNode(ExecutionPlan*, arangodb::velocypack::Slice const&);

  /// @brief return the type of the node
  NodeType getType() const override final { return UPDATE; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;
};

}  // namespace arangodb::aql
