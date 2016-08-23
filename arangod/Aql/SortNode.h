////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionNode.h"
#include "Aql/types.h"
#include "Aql/Variable.h"
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
  friend class SortBlock;
  friend class RedundantCalculationsReplacer;

 public:
  SortNode(ExecutionPlan* plan, size_t id, SortElementVector const& elements,
           bool stable)
      : ExecutionNode(plan, id), _elements(elements), _stable(stable) {}

  SortNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
           SortElementVector const& elements, bool stable);

  /// @brief return the type of the node
  NodeType getType() const override final { return SORT; }

  /// @brief whether or not the sort is stable
  inline bool isStable() const { return _stable; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&,
                          bool) const override final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    auto c = new SortNode(plan, _id, _elements, _stable);

    cloneHelper(c, plan, withDependencies, withProperties);

    return static_cast<ExecutionNode*>(c);
  }

  /// @brief estimateCost
  double estimateCost(size_t&) const override final;

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override final {
    std::vector<Variable const*> v;
    v.reserve(_elements.size());

    for (auto& p : _elements) {
      v.emplace_back(p.first);
    }
    return v;
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(
      std::unordered_set<Variable const*>& vars) const override final {
    for (auto& p : _elements) {
      vars.emplace(p.first);
    }
  }

  /// @brief get Variables Used Here including ASC/DESC
  SortElementVector const& getElements() const { return _elements; }

  /// @brief returns all sort information
  SortInformation getSortInformation(ExecutionPlan*,
                                     arangodb::basics::StringBuffer*) const;

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

 private:
  /// @brief pairs, consisting of variable and sort direction
  /// (true = ascending | false = descending)
  SortElementVector _elements;

  /// whether or not the sort is stable
  bool _stable;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
