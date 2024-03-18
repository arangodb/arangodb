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
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/IndexHint.h"
#include "Aql/SingleRowFetcher.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class Index;

namespace aql {
struct Collection;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;
struct Variable;

/// @brief class EnumerateCollectionNode
class EnumerateCollectionNode : public ExecutionNode,
                                public DocumentProducingNode,
                                public CollectionAccessingNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection name
 public:
  EnumerateCollectionNode(ExecutionPlan* plan, ExecutionNodeId id,
                          aql::Collection const* collection,
                          Variable const* outVariable, bool random,
                          IndexHint const& hint);

  EnumerateCollectionNode(ExecutionPlan* plan,
                          arangodb::velocypack::Slice base);

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief replaces variables in the internals of the execution node
  /// replacements are { old variable id => new variable }
  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  /// @brief replaces an attribute access in the internals of the execution
  /// node with a simple variable access
  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief the cost of an enumerate collection node is a multiple of the cost
  /// of its unique dependency
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief the node is only non-deterministic if it uses a random sort order
  bool isDeterministic() override final;

  /// @brief enable random iteration of documents in collection
  void setRandom();

  /// @brief user hint regarding which index to use
  IndexHint const& hint() const;

  void setProjections(Projections projections) override;

  bool isProduceResult() const override;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief whether or not we want a random document from the collection
  bool _random;

  /// @brief a possible hint from the user regarding which index to use
  IndexHint _hint;
};

}  // namespace aql
}  // namespace arangodb
