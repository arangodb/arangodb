////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionPlan.h"
#include "Containers/SmallUnorderedMap.h"
#include "VocBase/vocbase.h"

namespace arangodb::aql {
class Optimizer;

void createScatterGatherSnippet(
    ExecutionPlan& plan, TRI_vocbase_t* vocbase, ExecutionNode* node,
    bool isRootNode, std::vector<ExecutionNode*> const& nodeDependencies,
    std::vector<ExecutionNode*> const& nodeParents,
    SortElementVector const& elements, size_t numberOfShards,
    std::unordered_map<ExecutionNode*, ExecutionNode*> const& subqueries,
    Collection const* collection);

//// @brief enclose a node in SCATTER/GATHER
void insertScatterGatherSnippet(
    ExecutionPlan& plan, ExecutionNode* at,
    containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*> const&
        subqueries);

//// @brief find all subqueries in a plan and store a map from subqueries to
/// nodes
void findSubqueriesInPlan(
    ExecutionPlan& plan,
    containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*>& subqueries);

/// @brief scatter operations in cluster - send all incoming rows to all remote
/// clients
void scatterInClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                          OptimizerRule const&);

}  // namespace arangodb::aql
