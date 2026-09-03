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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AttributeNamePath.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/VarInfoMap.h"
#include "Aql/types.h"
#include "Containers/FlatHashSet.h"
#include "Utils/OperationOptions.h"

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
class ExecutionPlan;
class Projections;
class IndexHint;
class SortCondition;
struct Variable;
struct VarInfo;
struct NonConstExpressionContainer;
struct RegisterId;

namespace utils {

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
    bool& isAllCoveredByIndex, ReadOwnWrites readOwnWrites);

/// @brief Gets the best fitting index for an AQL condition.
/// note: the contents of  node  may be modified by this function if
/// an index is picked!!
bool getBestIndexHandleForFilterCondition(
    transaction::Methods& trx, aql::Collection const& collection,
    arangodb::aql::AstNode* node, arangodb::aql::Variable const* reference,
    size_t itemsInCollection, aql::IndexHint const& hint,
    std::shared_ptr<Index>& usedIndex, ReadOwnWrites readOwnWrites,
    bool onlyEdgeIndexes);

arangodb::aql::Collection const* getCollection(
    arangodb::aql::ExecutionNode const* node);

}  // namespace utils
}  // namespace aql
}  // namespace arangodb
