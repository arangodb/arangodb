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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AttributeNamePath.h"
#include "Aql/VarInfoMap.h"
#include "Aql/types.h"
#include "Containers/FlatHashSet.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace arangodb {
class Index;

namespace transaction {
class Methods;
}

namespace aql {

class Ast;
struct AstNode;
class AttributeNamePath;
struct Collection;
class ExecutionNode;
class IndexHint;
class SortCondition;
struct Variable;
struct VarInfo;
struct NonConstExpressionContainer;
struct RegisterId;

namespace utils {

// find projection attributes for variable v, starting from node n
// down to the root node of the plan/subquery.
// returns true if it is safe to reduce the full document data from
// "v" to only the projections stored in "attributes". returns false
// otherwise. if false is returned, the contents of "attributes" must
// be ignored by the caller.
// note: this function will not wipe "attributes" if there is already
// some data in it.
bool findProjections(ExecutionNode* n, Variable const* v,
                     std::string_view expectedAttribute,
                     bool excludeStartNodeFilterCondition,
                     containers::FlatHashSet<AttributeNamePath>& attributes);

/// @brief Gets the best fitting index for an AQL condition.
/// note: the contents of  root  may be modified by this function if
/// an index is picked!!
std::pair<bool, bool> getBestIndexHandlesForFilterCondition(
    transaction::Methods& trx, aql::Collection const& coll,
    arangodb::aql::Ast* ast, arangodb::aql::AstNode* root,
    arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition, size_t itemsInCollection,
    aql::IndexHint const& hint,
    std::vector<std::shared_ptr<Index>>& usedIndexes, bool& isSorted,
    bool& isAllCoveredByIndex);

/// @brief Gets the best fitting index for an AQL condition.
/// note: the contents of  node  may be modified by this function if
/// an index is picked!!
bool getBestIndexHandleForFilterCondition(
    transaction::Methods& trx, aql::Collection const& collection,
    arangodb::aql::AstNode* node, arangodb::aql::Variable const* reference,
    size_t itemsInCollection, aql::IndexHint const& hint,
    std::shared_ptr<Index>& usedIndex, bool onlyEdgeIndexes = false);

/// @brief Gets the best fitting index for an AQL sort condition
bool getIndexForSortCondition(aql::Collection const& coll,
                              arangodb::aql::SortCondition const* sortCondition,
                              arangodb::aql::Variable const* reference,
                              size_t itemsInIndex, aql::IndexHint const& hint,
                              std::vector<std::shared_ptr<Index>>& usedIndexes,
                              size_t& coveredAttributes);

NonConstExpressionContainer extractNonConstPartsOfIndexCondition(
    Ast* ast, VarInfoMap const& varInfo, bool evaluateFCalls, Index* index,
    AstNode const* condition, Variable const* indexVariable);

arangodb::aql::Collection const* getCollection(
    arangodb::aql::ExecutionNode const* node);

}  // namespace utils
}  // namespace aql
}  // namespace arangodb
