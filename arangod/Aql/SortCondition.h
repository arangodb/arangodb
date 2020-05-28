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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SORT_CONDITION_H
#define ARANGOD_AQL_SORT_CONDITION_H 1

#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Containers/HashSet.h"

namespace arangodb {

namespace aql {
struct AstNode;
class ExecutionPlan;

class SortCondition {
 public:
  SortCondition(SortCondition const&) = delete;
  SortCondition& operator=(SortCondition const&) = delete;

  /// @brief create an empty sort condition
  SortCondition();

  /// @brief create the sort condition
  SortCondition(ExecutionPlan* plan,
                std::vector<std::pair<Variable const*, bool>> const& sorts,
                std::vector<std::vector<arangodb::basics::AttributeName>> const& constAttributes,
                ::arangodb::containers::HashSet<std::vector<arangodb::basics::AttributeName>> const& nonNullAttributes,
                std::unordered_map<VariableId, AstNode const*> const& variableDefinitions);

  /// @brief destroy the sort condition
  ~SortCondition();

 public:
  /// @brief whether or not the condition consists only of attribute accesses
  inline bool isOnlyAttributeAccess() const { return _onlyAttributeAccess; }

  /// @brief whether or not all conditions have the same sort order
  inline bool isUnidirectional() const { return _unidirectional; }
  
  /// @brief whether or not all sort directions are ascending
  /// note that the return value of this function is only meaningful if the
  /// sort is unidirectional
  inline bool isAscending() const {
    TRI_ASSERT(isUnidirectional());
    return _ascending;
  }

  /// @brief whether or not all sort directions are descending
  /// this is the reverse of isAscending()
  inline bool isDescending() const { return !isAscending(); }

  /// @brief whether or not there are fields
  inline bool isEmpty() const { return _fields.empty(); }

  /// @brief number of attributes in condition
  inline size_t numAttributes() const { return _fields.size(); }

  /// @brief returns the number of attributes in the sort condition covered
  /// by the specified index fields
  size_t coveredAttributes(Variable const*,
                           std::vector<std::vector<arangodb::basics::AttributeName>> const&) const;

  /// @brief returns true if all attributes in the sort condition are proven
  /// to be non-null
  bool onlyUsesNonNullSortAttributes(
                           std::vector<std::vector<arangodb::basics::AttributeName>> const&) const;

  /// @brief  return the sort condition (as a tuple containing variable, AstNode
  /// and sort order) at `position`.
  /// `position` can  be a value between 0 and the result of
  /// SortCondition::numAttributes(). The bool attribute returned is whether
  /// the sort order is ascending (true) or descending (false)
  std::tuple<Variable const*, AstNode const*, bool> field(size_t position) const;

 private:
  struct SortField {
    Variable const* variable;
    std::vector<arangodb::basics::AttributeName> attributes;
    AstNode const* node;
    bool order;
  };

  ExecutionPlan* _plan;

  /// @brief fields used in the sort conditions
  std::vector<SortField> _fields;

  /// @brief const attributes
  std::vector<std::vector<arangodb::basics::AttributeName>> const _constAttributes;
  
  /// @brief non-null attributes
  ::arangodb::containers::HashSet<std::vector<arangodb::basics::AttributeName>> const _nonNullAttributes;

  /// @brief whether or not the sort is unidirectional
  bool _unidirectional;

  /// @brief whether the sort only consists of attribute accesses
  bool _onlyAttributeAccess;

  /// @brief whether or not all sorts are in ascending order.
  /// this is only meaningful if the sort is unidirectional
  bool _ascending;
};
}  // namespace aql
}  // namespace arangodb

#endif
