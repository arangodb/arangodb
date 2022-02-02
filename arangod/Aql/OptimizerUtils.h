////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/types.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

namespace arangodb {
class Index;

namespace aql {

class Ast;
struct AstNode;
struct Collection;
class IndexHint;
class SortCondition;
struct Variable;
struct VarInfo;
struct NonConstExpressionContainer;
struct RegisterId;

/// code that used to be in transaction::Methods
namespace utils {

/// @brief Gets the best fitting index for an AQL condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::pair<bool, bool> getBestIndexHandlesForFilterCondition(
    aql::Collection const& coll, arangodb::aql::Ast* ast,
    arangodb::aql::AstNode* root, arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition, size_t itemsInCollection,
    aql::IndexHint const& hint,
    std::vector<std::shared_ptr<Index>>& usedIndexes, bool& isSorted,
    bool& isAllCoveredByIndex);

/// @brief Gets the best fitting index for an AQL condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
bool getBestIndexHandleForFilterCondition(
    aql::Collection const& collection, arangodb::aql::AstNode*& node,
    arangodb::aql::Variable const* reference, size_t itemsInCollection,
    aql::IndexHint const& hint, std::shared_ptr<Index>& usedIndex,
    bool onlyEdgeIndexes = false);

/// @brief Gets the best fitting index for an AQL sort condition
/// note: the caller must have read-locked the underlying collection when
/// calling this method
bool getIndexForSortCondition(aql::Collection const& coll,
                              arangodb::aql::SortCondition const* sortCondition,
                              arangodb::aql::Variable const* reference,
                              size_t itemsInIndex, aql::IndexHint const& hint,
                              std::vector<std::shared_ptr<Index>>& usedIndexes,
                              size_t& coveredAttributes);

NonConstExpressionContainer extractNonConstPartsOfIndexCondition(
    Ast* ast, std::unordered_map<VariableId, VarInfo> const& varInfo,
    bool evaluateFCalls, bool sorted, AstNode const* condition,
    Variable const* indexVariable);

}  // namespace utils
}  // namespace aql
}  // namespace arangodb
