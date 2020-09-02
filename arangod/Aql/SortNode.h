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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SORT_NODE_H
#define ARANGOD_AQL_SORT_NODE_H 1

#include "Aql/Ast.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace basics {
class StringBuffer;
}

namespace aql {
class ExecutionBlock;
class ExecutionPlan;
class RedundantCalculationsReplacer;

/// @brief class SortNode
class SortNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  enum SorterType { Standard, ConstrainedHeap };
  static std::string const& sorterTypeName(SorterType);

 public:
  SortNode(ExecutionPlan* plan, ExecutionNodeId id, SortElementVector const& elements, bool stable)
      : ExecutionNode(plan, id),
        _reinsertInCluster(true),
        _elements(elements),
        _stable(stable) {}

  SortNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
           SortElementVector const& elements, bool stable);

  /// @brief if non-zero, limits the number of elements that the node will return
  void setLimit(size_t limit) { _limit = limit; }

  size_t limit() const noexcept { return _limit; }

  /// @brief return the type of the node
  NodeType getType() const override final { return SORT; }

  /// @brief whether or not the sort is stable
  inline bool isStable() const { return _stable; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    return cloneHelper(std::make_unique<SortNode>(plan, _id, _elements, _stable),
                       withDependencies, withProperties);
  }

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final {
    for (auto& p : _elements) {
      vars.emplace(p.var);
    }
  }

  /// @brief get Variables Used Here including ASC/DESC
  SortElementVector const& elements() const { return _elements; }

  /// @brief returns all sort information
  SortInformation getSortInformation(ExecutionPlan*, arangodb::basics::StringBuffer*) const;

  std::vector<std::pair<ExecutionNode*, bool>> getCalcNodePairs();

  /// @brief simplifies the expressions of the sort node
  /// this will sort expressions if they are constant
  /// the method will return true if all sort expressions were removed after
  /// simplification, and false otherwise
  bool simplify(ExecutionPlan*);

  /// @brief removes the first count conditions from the sort condition
  /// this can be used if the first conditions of the condition are constant
  /// values (e.g. when a FILTER condition exists that guarantees this)
  void removeConditions(size_t count);

  SorterType sorterType() const;

  /// @brief if this node is needed on DBServers in cluster.
  /// if set to false that means some optimizer rule
  /// has already included sorting in some other node and
  /// this node is left in plan only in sake of GatherNode 
  /// to properly handle merging.
  bool _reinsertInCluster;

 private:
  /// @brief pairs, consisting of variable and sort direction
  /// (true = ascending | false = descending)
  SortElementVector _elements;

  /// whether or not the sort is stable
  bool _stable;

  /// the maximum number of items to return if non-zero; if zero, unlimited
  size_t _limit = 0;
};

}  // namespace aql
}  // namespace arangodb

#endif
