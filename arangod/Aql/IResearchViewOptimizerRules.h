////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_VIEW_OPTIMIZER_RULES_H
#define ARANGOD_IRESEARCH__IRESEARCH_VIEW_OPTIMIZER_RULES_H 1

#include <memory>

namespace arangodb {

namespace aql {
class Optimizer;
struct OptimizerRule;
class ExecutionPlan;
}  // namespace aql

namespace iresearch {

/// @brief moves document materialization from view nodes to materialize nodes
void lateDocumentMaterializationArangoSearchRule(arangodb::aql::Optimizer* opt,
                     std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                     arangodb::aql::OptimizerRule const& rule);

/// @brief no document materialization for view nodes if stored values contain all fields
void noDocumentMaterializationArangoSearchRule(arangodb::aql::Optimizer* opt,
                     std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                     arangodb::aql::OptimizerRule const& rule);

/// @brief move filters and sort conditions into views
void handleViewsRule(arangodb::aql::Optimizer* opt,
                     std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                     arangodb::aql::OptimizerRule const& rule);

/// @brief scatter view query in cluster
/// this rule inserts scatter, gather and remote nodes so operations on sharded
/// views
void scatterViewInClusterRule(arangodb::aql::Optimizer* opt,
                              std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                              arangodb::aql::OptimizerRule const& rule);

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_VIEW_OPTIMIZER_RULES_H
