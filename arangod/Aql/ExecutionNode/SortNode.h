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

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/SortElement.h"
#include "Aql/SortInformation.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Slice.h>

#include <string>
#include <string_view>

namespace arangodb::aql {

class ExecutionBlock;
class ExecutionPlan;

/// @brief class SortNode
class SortNode : public ExecutionNode {
  friend class ExecutionBlock;

 public:
  enum class SorterType { kStandard, kConstrainedHeap };

  SortNode(ExecutionPlan* plan, ExecutionNodeId id, SortElementVector elements,
           bool stable);

  SortNode(ExecutionPlan* plan, arangodb::velocypack::Slice base,
           SortElementVector elements, bool stable);

  /// @brief if non-zero, limits the number of elements that the node will
  /// return
  void setLimit(size_t limit) { _limit = limit; }

  size_t limit() const noexcept { return _limit; }

  /// @brief return the type of the node
  NodeType getType() const override final { return SORT; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief whether or not the sort is stable
  bool isStable() const noexcept { return _stable; }

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  /// @brief replaces an attribute access in the internals of the execution
  /// node with a simple variable access
  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief get Variables Used Here including ASC/DESC
  SortElementVector const& elements() const noexcept { return _elements; }

  /// @brief returns all sort information
  SortInformation getSortInformation() const;

  /// @brief simplifies the expressions of the sort node
  /// this will sort expressions if they are constant
  /// the method will return true if all sort expressions were removed after
  /// simplification, and false otherwise
  bool simplify(ExecutionPlan* plan);

  SorterType sorterType() const noexcept;

  bool reinsertInCluster() const noexcept { return _reinsertInCluster; }

  void dontReinsertInCluster() noexcept { _reinsertInCluster = false; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  static std::string_view sorterTypeName(SorterType type) noexcept;

  /// @brief pairs, consisting of variable and sort direction
  /// (true = ascending | false = descending)
  SortElementVector _elements;

  /// whether or not the sort is stable
  bool _stable;

  /// @brief if this node is needed on DBServers in cluster.
  /// if set to false that means some optimizer rule
  /// has already included sorting in some other node and
  /// this node is left in plan only in sake of GatherNode
  /// to properly handle merging.
  bool _reinsertInCluster;

  /// the maximum number of items to return if non-zero; if zero, unlimited
  size_t _limit = 0;
};

}  // namespace arangodb::aql
