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
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinOrderSearch.h"

#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Variable.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <utility>

namespace arangodb::aql {

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
namespace {

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
}  // namespace

#endif

void optimizeJoinOrder(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                       OptimizerRule const& rule) {
  auto estimator = makeDefaultJoinCostEstimator(*plan);
  bool modified = false;

  ExecutionNode* n = plan->root()->getSingleton();
  while (n != nullptr) {
    if (n->getType() != ExecutionNode::ENUMERATE_COLLECTION) {
      n = n->getFirstParent();
      continue;
    }

    ExecutionNode* firstEnumeration = n;
    ExecutionNode* next = nullptr;
    JoinGraph graph = buildJoinGraph(plan.get(), firstEnumeration, next);

    if (graph.hasJoin() && !graph.hasNonDeterministicCalculation) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      traceJoinGraph(graph);
#endif
      auto const writtenOrder = collectEnumerationOrder(firstEnumeration, next);
      if (auto chosen = chooseJoinOrder(graph, *estimator, writtenOrder);
          chosen.has_value()) {
        rewriteJoinGraph(*plan, firstEnumeration, next, *chosen);
        modified = true;
      }
    }

    n = next;
  }

  if (modified) {
    // Conditional on `modified`, not on this rule being enabled: when it
    // declines, interchange must still run, or opting in could leave a query
    // less optimized than the default.
    opt->disableRules(plan.get(), [](OptimizerRule const& r) {
      return r.level == OptimizerRule::interchangeAdjacentEnumerationsRule;
    });
  }

  // `modified` decides whether the rule appears in explain's applied-rules
  // list, which hasAppliedRule() also reads.
  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
