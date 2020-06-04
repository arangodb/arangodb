////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_INDEX_NODE_OPTIMIZER_RULES_H
#define ARANGOD_AQL_INDEX_NODE_OPTIMIZER_RULES_H 1

#include <memory>

namespace arangodb {

namespace aql {
class Optimizer;
struct OptimizerRule;
class ExecutionPlan;

/// @brief moves document materialization from index nodes to materialize nodes
void lateDocumentMaterializationRule(arangodb::aql::Optimizer* opt,
                     std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                     arangodb::aql::OptimizerRule const& rule);

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_INDEX_NODE_OPTIMIZER_RULES_H
