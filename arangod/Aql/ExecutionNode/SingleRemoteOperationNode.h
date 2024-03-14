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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ModificationOptions.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <memory>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {
class ExecutionBlock;
class ExecutionEngine;
class ExecutionPlan;
struct Collection;

/// @brief class RemoteNode
class SingleRemoteOperationNode final : public ExecutionNode,
                                        public CollectionAccessingNode {
  friend class ExecutionBlock;
  friend class SingleRemoteOperationBlock;
  /// @brief constructor with an id
 public:
  SingleRemoteOperationNode(ExecutionPlan* plan, ExecutionNodeId id,
                            NodeType mode, bool replaceIndexNode,
                            std::string const& key,
                            aql::Collection const* collection,
                            ModificationOptions const& options,
                            Variable const* update, Variable const* out,
                            Variable const* OLD, Variable const* NEW);

  // We probably do not need this, because the rule will only be used on the
  // coordinator
  SingleRemoteOperationNode(ExecutionPlan* plan,
                            arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return REMOTESINGLE; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  std::string const& key() const { return _key; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  // whether we replaced an index node
  bool _replaceIndexNode;

  /// the key of the document we're intending to work with
  std::string _key;

  NodeType _mode;
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
