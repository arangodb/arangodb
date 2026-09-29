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
////////////////////////////////////////////////////////////////////////////////

#include "OptimizeJoinOrder.h"

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinGraph.h"

#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/Variable.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <memory>
#include <utility>

namespace arangodb::aql {

namespace {

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void traceJoinGraph(JoinGraph const& graph) {
  auto components = graph.connectedComponents();
  LOG_TOPIC("a7f01", TRACE, Logger::AQL)
      << "optimize-join-order: join graph with " << graph.nodes.size()
      << " node(s), " << graph.edges.size() << " edge(s), "
      << graph.residuals.size() << " residual(s), " << components.size()
      << " component(s)";
  for (auto const& e : graph.edges) {
    LOG_TOPIC("a7f02", TRACE, Logger::AQL)
        << "optimize-join-order:   edge "
        << e.from->executionNode->outVariable()->name << " <-> "
        << e.to->executionNode->outVariable()->name;
  }
}
#endif

}  // namespace

void optimizeJoinOrder(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                       OptimizerRule const& rule) {
  // Scaffolding: construct the join graph(s) for the plan. This does
  // NOT reorder joins and does NOT modify the plan yet; the reordering
  // algorithm is a follow-up task. We report the rule as "applied" only when we
  // actually discovered a join so that it is observable via `explain`.
  bool foundJoin = false;

  ExecutionNode* n = plan->root()->getSingleton();
  while (n != nullptr) {
    if (n->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
      ExecutionNode* next = nullptr;

      // This graph is discarded now, but will be retained and used for join
      // reordering later.
      JoinGraph graph = buildJoinGraph(plan.get(), n, next);
      if (graph.hasJoin()) {
        foundJoin = true;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        traceJoinGraph(graph);
#endif
      }
      // continue scanning after the run that we just consumed
      n = next;
    } else {
      n = n->getFirstParent();
    }
  }

  opt->addPlan(std::move(plan), rule, foundJoin);
}

}  // namespace arangodb::aql
