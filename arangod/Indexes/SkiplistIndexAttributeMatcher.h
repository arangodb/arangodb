////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_SKIPLIST_INDEX_ATTRIBUTE_MATCHER_H
#define ARANGOD_SKIPLIST_INDEX_ATTRIBUTE_MATCHER_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace aql {
class Ast;
struct AstNode;
class SortCondition;
struct Variable;
}

class Index;

/// Contains code for in-memory skiplist indexes (MMFilesSkiplistIndex)
namespace SkiplistIndexAttributeMatcher {

bool supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                             arangodb::Index const* index,
                             arangodb::aql::AstNode const* node,
                             arangodb::aql::Variable const* reference,
                             size_t itemsInIndex, size_t& estimatedItems,
                             double& estimatedCost);

bool supportsSortCondition(arangodb::Index const*,
                           arangodb::aql::SortCondition const* sortCondition,
                           arangodb::aql::Variable const* reference,
                           size_t itemsInIndex, double& estimatedCost,
                           size_t& coveredAttributes);

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* specializeCondition(
    arangodb::Index const*, arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference);

void matchAttributes(
    arangodb::Index const*, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    size_t& values, std::unordered_set<std::string>& nonNullAttributes,
    bool isExecution);

/// @brief whether or not the access fits
bool accessFitsIndex(
    arangodb::Index const* idx, arangodb::aql::AstNode const* access,
    arangodb::aql::AstNode const* other, arangodb::aql::AstNode const* op,
    arangodb::aql::Variable const* reference,
    std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&
        found,
    std::unordered_set<std::string>& nonNullAttributes, bool isExecution);

bool isDuplicateOperator(arangodb::aql::AstNode const* node,
                         std::unordered_set<int> const& operatorsFound);
};
}

#endif
