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

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Aql/Variable.h"
#include "Transaction/StandaloneContext.h"

#include "velocypack/Parser.h"

#include "../IResearch/common.h"
#include "../Mocks/Servers.h"
#include "Async/async.h"

#include <memory>
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

}  // namespace arangodb::tests::aql
