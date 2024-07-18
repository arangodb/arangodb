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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Optimizer/Rule/OptimizerRulesLateMaterializedCommon.h"

#include <memory>

namespace arangodb {
namespace aql {

class Optimizer;
struct OptimizerRule;
class ExecutionPlan;

}  // namespace aql
namespace iresearch {

// Find immutable part of search condition for subqueries or inner loops
// Regroup them to the two parts: immutable mutable
void immutableSearchCondition(aql::Optimizer* opt,
                              std::unique_ptr<aql::ExecutionPlan> plan,
                              aql::OptimizerRule const& rule);

// Moves document materialization from view nodes to materialize nodes.
void lateDocumentMaterializationArangoSearchRule(
    aql::Optimizer* opt, std::unique_ptr<aql::ExecutionPlan> plan,
    aql::OptimizerRule const& rule);

// Move search and scorers into views
// Replace variables to avoid materialization document at all
void handleViewsRule(aql::Optimizer* opt,
                     std::unique_ptr<aql::ExecutionPlan> plan,
                     aql::OptimizerRule const& rule);

// Move constrained sort into views.
void handleConstrainedSortInView(aql::Optimizer* opt,
                                 std::unique_ptr<aql::ExecutionPlan> plan,
                                 aql::OptimizerRule const& rule);

// Scatter view query in cluster this rule inserts scatter, gather and
// remote nodes so operations on sharded views.
void scatterViewInClusterRule(aql::Optimizer* opt,
                              std::unique_ptr<aql::ExecutionPlan> plan,
                              aql::OptimizerRule const& rule);

}  // namespace iresearch
}  // namespace arangodb
