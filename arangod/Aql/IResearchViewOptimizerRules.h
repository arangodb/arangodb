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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "Aql/LateMaterializedOptimizerRulesCommon.h"

namespace arangodb {

namespace aql {
class Optimizer;
struct OptimizerRule;
class ExecutionPlan;
}  // namespace aql

namespace iresearch {

// Moves document materialization from view nodes to materialize nodes.
void lateDocumentMaterializationArangoSearchRule(
    arangodb::aql::Optimizer* opt,
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
    arangodb::aql::OptimizerRule const& rule);

// Move search and scorers into views.
void handleViewsRule(arangodb::aql::Optimizer* opt,
                     std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                     arangodb::aql::OptimizerRule const& rule);

// Move constrained sort into views.
void handleConstrainedSortInView(
    arangodb::aql::Optimizer* opt,
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
    arangodb::aql::OptimizerRule const& rule);

// Scatter view query in cluster this rule inserts scatter, gather and
// remote nodes so operations on sharded views.
void scatterViewInClusterRule(
    arangodb::aql::Optimizer* opt,
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
    arangodb::aql::OptimizerRule const& rule);

}  // namespace iresearch
}  // namespace arangodb
