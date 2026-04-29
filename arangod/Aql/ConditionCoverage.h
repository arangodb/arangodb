////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Containers/HashSet.h"

namespace arangodb {
class Index;

namespace aql {
class Ast;
struct AstNode;
class ExecutionPlan;
struct Variable;

bool isConditionCoveredBy(ExecutionPlan const* plan, Variable const* variable,
                          AstNode const* condition,
                          AstNode const* otherAndNode);

bool extractSingleAndNodes(AstNode const* root, AstNode const* condition,
                           AstNode const*& andNode,
                           AstNode const*& conditionAndNode);

AstNode* rebuildConditionWithoutMembers(
    Ast* ast, AstNode const* andNode,
    containers::HashSet<size_t> const& toRemove);

containers::HashSet<size_t> collectOverlappingMembersForTraversal(
    ExecutionPlan const* plan, Variable const* variable, AstNode const* andNode,
    AstNode const* otherAndNode, bool isPathCondition);

}  // namespace aql
}  // namespace arangodb