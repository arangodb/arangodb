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
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <cstdint>
#include <memory>
#include <utility>

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

/// @brief class GatherNode
class GatherNode final : public ExecutionNode {
  friend class ExecutionBlock;

 public:
  enum class SortMode : uint32_t { MinElement, Heap, Default };

  enum class Parallelism : uint8_t { Undefined = 0, Serial = 2, Parallel = 4 };

  /// @brief inspect dependencies starting from a specified 'node'
  /// and return first corresponding collection within
  /// a diamond if so exist
  static Collection const* findCollection(GatherNode const& node) noexcept;

  /// @returns sort mode for the specified number of shards
  static SortMode evaluateSortMode(
      size_t numberOfShards, size_t shardsRequiredForHeapMerge = 5) noexcept;

  static Parallelism evaluateParallelism(Collection const& collection) noexcept;

  /// @brief constructor with an id
  GatherNode(ExecutionPlan* plan, ExecutionNodeId id, SortMode sortMode,
             Parallelism parallelism = Parallelism::Undefined) noexcept;

  GatherNode(ExecutionPlan*, arangodb::velocypack::Slice const& base,
             SortElementVector const& elements);

  /// @brief return the type of the node
  NodeType getType() const override final { return GATHER; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

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

  /// @brief get Variables used here including ASC/DESC
  SortElementVector const& elements() const { return _elements; }
  SortElementVector& elements() { return _elements; }

  void elements(SortElementVector const& src) { _elements = src; }

  SortMode sortMode() const noexcept { return _sortmode; }
  void sortMode(SortMode sortMode) noexcept { _sortmode = sortMode; }

  void setConstrainedSortLimit(size_t limit) noexcept;

  size_t constrainedSortLimit() const noexcept;

  bool isSortingGather() const noexcept;

  void setParallelism(Parallelism value);

  Parallelism parallelism() const noexcept { return _parallelism; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief the underlying database
  TRI_vocbase_t* _vocbase;

  /// @brief sort elements, variable, ascending flags and possible attribute
  /// paths.
  SortElementVector _elements;

  /// @brief sorting mode
  SortMode _sortmode;

  /// @brief parallelism
  Parallelism _parallelism;

  /// @brief In case this was created from a constrained heap sorting node, this
  /// is its limit (which is greater than zero). Otherwise, it's zero.
  size_t _limit;
};

auto toString(GatherNode::SortMode mode) noexcept -> std::string_view;
auto toString(GatherNode::Parallelism) noexcept -> std::string_view;

}  // namespace aql
}  // namespace arangodb
