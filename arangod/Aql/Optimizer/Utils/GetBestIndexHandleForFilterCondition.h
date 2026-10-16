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

#include "Aql/SortCondition.h"
#include "Utils/OperationOptions.h"

#include <memory>
#include <vector>
namespace arangodb {
class Index;

namespace transaction {
class Methods;
}

namespace aql {

struct Collection;
struct AstNode;
struct Variable;
class IndexHint;
class SortCondition;

namespace optimizer {
/// @brief Gets the best fitting index for an AQL condition.
/// note: the contents of  root  may be modified by this function if
/// an index is picked!!
std::pair<bool, bool> getBestIndexHandlesForFilterCondition(
    transaction::Methods& trx, Collection const& coll, Ast* ast, AstNode* root,
    Variable const* reference, SortCondition const* sortCondition,
    size_t itemsInCollection, IndexHint const& hint,
    std::vector<std::shared_ptr<Index>>& usedIndexes, bool& isSorted,
    bool& isAllCoveredByIndex, ReadOwnWrites readOwnWrites);

/// @brief Gets the best fitting index for an AQL condition.
/// note: the contents of  node  may be modified by this function if
/// an index is picked!!
bool getBestIndexHandleForFilterCondition(
    transaction::Methods& trx, Collection const& collection, AstNode* node,
    Variable const* reference, size_t itemsInCollection, IndexHint const& hint,
    std::shared_ptr<Index>& usedIndex, ReadOwnWrites readOwnWrites,
    bool onlyEdgeIndexes);

}  // namespace optimizer
}  // namespace aql
}  // namespace arangodb
