////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Ast.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ModificationOptions.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "VocBase/voc-types.h"

#include <utility>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {
class ExecutionBlock;
class ExecutionPlan;
struct Collection;

/// @brief class RemoteNode
class MultipleRemoteModificationNode final : public ExecutionNode,
                                             public CollectionAccessingNode {
  friend class ExecutionBlock;
  friend class SingleRemoteOperationBlock;
  /// @brief constructor with an id
 public:
  MultipleRemoteModificationNode(ExecutionPlan* plan, ExecutionNodeId id,
                                 aql::Collection const* collection,
                                 ModificationOptions const& options,
                                 Variable const* inVariable,
                                 Variable const* outVariable,
                                 Variable const* OLD, Variable const* NEW);

  // We probably do not need this, because the rule will only be used on the
  // coordinator
  MultipleRemoteModificationNode(ExecutionPlan* plan,
                                 arangodb::velocypack::Slice const& base)
      : ExecutionNode(plan, base), CollectionAccessingNode(plan, base) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "multiple remote operation node deserialization not implemented.");
  }

  /// @brief return the type of the node
  NodeType getType() const override final { return REMOTE_MULTIPLE; }

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&)
      const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    auto n = std::make_unique<MultipleRemoteModificationNode>(
        plan, _id, collection(), _options, _inVariable, _outVariable,
        _outVariableOld, _outVariableNew);
    CollectionAccessingNode::cloneInto(*n);
    return cloneHelper(std::move(n), withDependencies, withProperties);
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  Variable const* _inVariable;
  Variable const* _outVariable;

  /// @brief output variable ($OLD)
  Variable const* _outVariableOld;

  /// @brief output variable ($NEW)
  Variable const* _outVariableNew;

  /// @brief modification operation options
  ModificationOptions _options;
};

}  // namespace aql
}  // namespace arangodb
