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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gtest/gtest.h"

#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinCostEstimator.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinStatistics.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Aql/Variable.h"
#include "Transaction/StandaloneContext.h"

#include "velocypack/Parser.h"

#include "../IResearch/common.h"
#include "../Mocks/Servers.h"
#include "Async/async.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb::tests::aql {

// Prepares a plan for `query` without any optional optimizer rules, so that
// the plain EnumerateCollection / Filter nodes survive (this mirrors what the
// optimize-join-order rule sees at its position in the pipeline).
inline std::shared_ptr<arangodb::aql::Query> prepareJoinPlan(
    mocks::MockAqlServer& server, std::string const& query) {
  auto ctx = std::make_shared<transaction::StandaloneContext>(
      server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  auto options =
      velocypack::Parser::fromJson(R"({"optimizer":{"rules":["-all"]}})");
  auto q = arangodb::aql::Query::create(
      std::move(ctx), arangodb::aql::QueryString(query), nullptr,
      arangodb::aql::QueryOptions(options->slice()));
  waitForAsync(q->prepareQuery());
  return q;
}

// Like prepareJoinPlan(), but leaves the query in PLAN_OPTIMIZATION instead
// of EXECUTION. prepareJoinPlan()'s prepareQuery() call always finishes by
// entering EXECUTION, which is fine for consumers that only walk the plan's
// node structure -- but IndexJoinStatistics itself calls
// Query::trxForOptimization(), an optimization-time API that asserts (under
// maintainer mode) that the query has not started executing. Use this
// instead of prepareJoinPlan() wherever an IndexJoinStatistics is
// constructed directly over the resulting plan.
inline std::shared_ptr<arangodb::aql::Query> prepareJoinPlanForStatistics(
    mocks::MockAqlServer& server, std::string const& query) {
  auto ctx = std::make_shared<transaction::StandaloneContext>(
      server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  auto options =
      velocypack::Parser::fromJson(R"({"optimizer":{"rules":["-all"]}})");
  auto q = arangodb::aql::Query::create(
      std::move(ctx), arangodb::aql::QueryString(query), nullptr,
      arangodb::aql::QueryOptions(options->slice()));
  q->prepareOptimizedPlanForTests();
  return q;
}

// Walks from the singleton towards the root and returns the first
// EnumerateCollection node (i.e. the bottom-most enumeration).
inline arangodb::aql::ExecutionNode* firstEnumeration(
    arangodb::aql::ExecutionPlan const* plan) {
  for (auto* n = plan->root()->getSingleton(); n != nullptr;
       n = n->getFirstParent()) {
    if (n->getType() == arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION) {
      return n;
    }
  }
  return nullptr;
}

inline arangodb::aql::JoinGraph buildGraph(arangodb::aql::Query const& q) {
  auto* plan = q.plan();
  auto* first = firstEnumeration(plan);
  EXPECT_NE(first, nullptr);
  arangodb::aql::ExecutionNode* next = nullptr;
  return arangodb::aql::buildJoinGraph(plan, first, next);
}

// Builds one JoinGraph per maximal run of adjacent enumerations, mirroring
// the spine walk of the optimizeJoinOrder rule itself.
inline std::vector<arangodb::aql::JoinGraph> buildAllGraphs(
    arangodb::aql::Query const& q) {
  auto* plan = q.plan();
  std::vector<arangodb::aql::JoinGraph> graphs;
  for (auto* n = plan->root()->getSingleton(); n != nullptr;) {
    if (n->getType() == arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION) {
      arangodb::aql::ExecutionNode* next = nullptr;
      graphs.emplace_back(arangodb::aql::buildJoinGraph(plan, n, next));
      n = next;
    } else {
      n = n->getFirstParent();
    }
  }
  return graphs;
}

inline arangodb::aql::JoinGraph::Node* nodeByName(arangodb::aql::JoinGraph& g,
                                                  std::string_view name) {
  for (auto& [var, node] : g.nodes) {
    if (var->name == name) {
      return &node;
    }
  }
  return nullptr;
}

// Scripted statistics, keyed by out-variable name so tests read declaratively.
// This is what makes the formula tests exact: real collections cannot produce
// an exact non-unique distinct count.
class FakeJoinStatistics final : public arangodb::aql::JoinStatistics {
 public:
  // variable name -> document count
  std::map<std::string, double, std::less<>> counts;
  // variable name -> "attr,attr" key -> distinct estimate
  std::map<std::string, std::map<std::string, arangodb::aql::DistinctEstimate>,
           std::less<>>
      distinct;
  // variable name -> attribute names that an index can probe by
  std::map<std::string, std::set<std::string>, std::less<>> indexed;

  // Joins an attribute set into a stable lookup key: {["a"],["b","c"]}
  // becomes "a,b.c". Paths are sorted so callers need not match ordering.
  static std::string keyFor(
      std::span<arangodb::aql::AttributePath const> attributes) {
    std::vector<std::string> parts;
    parts.reserve(attributes.size());
    for (auto const& path : attributes) {
      std::string joined;
      for (auto const& component : path) {
        if (!joined.empty()) {
          joined += '.';
        }
        joined += component;
      }
      parts.emplace_back(std::move(joined));
    }
    std::sort(parts.begin(), parts.end());
    std::string key;
    for (auto const& part : parts) {
      if (!key.empty()) {
        key += ',';
      }
      key += part;
    }
    return key;
  }

  static std::string nameOf(arangodb::aql::JoinGraph::Node const& node) {
    return node.executionNode->outVariable()->name;
  }

  auto documentCount(arangodb::aql::JoinGraph::Node const& node) const
      -> double override {
    auto it = counts.find(nameOf(node));
    return it == counts.end() ? 0.0 : it->second;
  }

  auto distinctValues(arangodb::aql::JoinGraph::Node const& node,
                      std::span<arangodb::aql::AttributePath const> attributes)
      const -> arangodb::aql::DistinctEstimate override {
    if (attributes.empty()) {
      return {1.0, false};
    }
    auto outer = distinct.find(nameOf(node));
    if (outer != distinct.end()) {
      auto inner = outer->second.find(keyFor(attributes));
      if (inner != outer->second.end()) {
        return inner->second;
      }
    }
    return {1.0, true};
  }

  auto hasIndexCovering(
      arangodb::aql::JoinGraph::Node const& node,
      std::span<arangodb::aql::AttributePath const> attributes) const
      -> bool override {
    auto it = indexed.find(nameOf(node));
    if (it == indexed.end()) {
      return false;
    }
    for (auto const& path : attributes) {
      if (!path.empty() && it->second.contains(std::string{path.front()})) {
        return true;
      }
    }
    return false;
  }
};

// A cost estimator whose numbers are dictated by the test: `seedCost[v]` is the
// cost of starting at v, `stepCost[v]` the cost added when v is appended.
// Cardinality is not modelled because the greedy selects on cost.
class FakeCostEstimator final : public arangodb::aql::JoinCostEstimator {
 public:
  std::map<std::string, double, std::less<>> seedCost;
  std::map<std::string, double, std::less<>> stepCost;
  // set to true to make every estimate report a defaulted statistic
  bool defaulted = false;
  // out-variable names of vertices whose statistic reports as defaulted
  // independently of the blanket `defaulted` flag above -- lets a test mark
  // just one vertex (or component) as unindexed while the rest of the graph
  // stays confident.
  std::set<std::string> defaultedVertices;
  // records how many connecting edges `extend` was offered for each vertex it
  // added, keyed by that vertex's out-variable name, so tests can assert on
  // it directly instead of only reasoning about it from cost behaviour.
  mutable std::map<std::string, size_t> edgesSeen;

  static std::string nameOf(arangodb::aql::JoinGraph::Node const& node) {
    return node.executionNode->outVariable()->name;
  }

  auto seed(arangodb::aql::JoinGraph::Node const& start) const
      -> arangodb::aql::JoinEstimate override {
    auto it = seedCost.find(nameOf(start));
    return {
        .cardinality = 1.0,
        .cost = it == seedCost.end() ? 1.0 : it->second,
        .defaulted = defaulted || defaultedVertices.contains(nameOf(start))};
  }

  auto extend(arangodb::aql::JoinEstimate const& prefix,
              arangodb::aql::JoinGraph::Node const& next,
              std::span<arangodb::aql::JoinGraph::Edge const* const> connecting)
      const -> arangodb::aql::JoinEstimate override {
    edgesSeen[nameOf(next)] = connecting.size();
    auto it = stepCost.find(nameOf(next));
    double step = it == stepCost.end() ? 1.0 : it->second;
    // a cross product is charged double, so tests can see components being
    // kept together
    if (connecting.empty()) {
      step *= 2.0;
    }
    return {.cardinality = prefix.cardinality,
            .cost = prefix.cost + step,
            .defaulted = prefix.defaulted || defaulted ||
                         defaultedVertices.contains(nameOf(next))};
  }
};

}  // namespace arangodb::tests::aql
