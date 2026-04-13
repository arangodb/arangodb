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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"

namespace arangodb::aql {
class Optimizer;
class SubqueryNode;

class QueryContext;
struct Collection;

/// @brief split and-combined filters and break them into smaller parts
void splitFiltersRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                      OptimizerRule const&);

/// @brief replace single document operations in cluster by special handling
void substituteClusterSingleDocumentOperationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan, OptimizerRule const&);

/// @brief replace multiple document operations in cluster by special handling
void substituteClusterMultipleDocumentOperationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan, OptimizerRule const&);

/// @brief replace legacy JS functions in the plan.
void replaceNearWithinFulltextRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                   OptimizerRule const&);

/// @brief replace LIKE function with range scan where possible
void replaceLikeWithRangeRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                              OptimizerRule const&);

void joinIndexNodesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                        OptimizerRule const&);

void useVectorIndexRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                        OptimizerRule const&);

void pushFilterIntoEnumerateNear(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                 OptimizerRule const&);

}  // namespace arangodb::aql