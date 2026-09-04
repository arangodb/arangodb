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

#include "JoinGraphTestHelper.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace arangodb::aql;

namespace arangodb::tests::aql {
namespace {
// Builds one JoinGraph per maximal run of adjacent enumerations, mirroring
// the spine walk of the optimizeJoinOrder rule itself.
std::vector<JoinGraph> buildAllGraphs(Query const& q) {
  auto* plan = q.plan();
  std::vector<JoinGraph> graphs;
  for (auto* n = plan->root()->getSingleton(); n != nullptr;) {
    if (n->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
      ExecutionNode* next = nullptr;
      graphs.emplace_back(buildJoinGraph(plan, n, next));
      n = next;
    } else {
      n = n->getFirstParent();
    }
  }
  return graphs;
}

class JoinGraphTest : public testing::Test {
 protected:
  mocks::MockAqlServer server;

  JoinGraphTest() {
    auto& vocbase = server.getSystemDatabase();
    for (auto const& name : {"c1", "c2", "c3"}) {
      auto json = velocypack::Parser::fromJson(std::string{R"({"name":")"} +
                                               name + "\"}");
      vocbase.createCollection(json->slice());
    }
  }

  std::shared_ptr<Query> prepare(std::string const& query) {
    return prepareJoinPlan(server, query);
  }
};
}  // namespace

TEST_F(JoinGraphTest, linear_three_way_chain) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 3u);
  EXPECT_EQ(g.edges.size(), 2u);
  EXPECT_TRUE(g.residuals.empty());
  EXPECT_TRUE(g.hasJoin());
  EXPECT_EQ(g.connectedComponents().size(), 1u);
}

TEST_F(JoinGraphTest, equijoin_edge_records_attribute_paths) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);

  ASSERT_EQ(g.edges.size(), 1u);
  auto const& e = g.edges.front();
  ASSERT_EQ(e.fromAttributes.size(), 1u);
  ASSERT_EQ(e.toAttributes.size(), 1u);
  // one side is {"x"} and the other {"y"} (orientation depends on map order)
  auto const& fromPath = e.fromAttributes.front();
  auto const& toPath = e.toAttributes.front();
  ASSERT_EQ(fromPath.size(), 1u);
  ASSERT_EQ(toPath.size(), 1u);
  std::vector got{fromPath[0], toPath[0]};
  std::ranges::sort(got);
  EXPECT_EQ(got, (std::vector<std::string_view>{"x", "y"}));
}

TEST_F(JoinGraphTest, constant_restriction_becomes_node_condition) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.k == 'v' RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 2u);
  EXPECT_EQ(g.edges.size(), 1u);
  EXPECT_TRUE(g.residuals.empty());

  auto const* a = nodeByName(g, "a");
  ASSERT_NE(a, nullptr);
  ASSERT_EQ(a->conditions.size(), 1u);
  ASSERT_EQ(a->conditions.front().size(), 1u);
  EXPECT_EQ(a->conditions.front().front(), "k");
}

TEST_F(JoinGraphTest, non_equijoin_predicate_becomes_residual) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.p < b.q RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.edges.size(), 1u);
  EXPECT_FALSE(g.residuals.empty());
}

TEST_F(JoinGraphTest, equality_within_one_variable_becomes_residual) {
  // `a.p == a.q` compares two attributes of one document. It cannot be
  // probed -- the lookup key would come from the row not yet read -- so it is
  // a per-row filter, not a join, and must not produce a self-loop edge.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FILTER a.p == a.q RETURN [a, b]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.edges.size(), 1u);
  for (auto const& edge : g.edges) {
    EXPECT_NE(edge.from, edge.to);
  }
  EXPECT_EQ(g.residuals.size(), 1u);
  EXPECT_TRUE(nodeByName(g, "a")->conditions.empty());

  // and on its own it is not a join at all
  auto alone = buildGraph(
      *prepare("FOR a IN c1 FOR b IN c2 FILTER a.p == a.q RETURN [a, b]"));
  EXPECT_FALSE(alone.hasJoin());
  EXPECT_EQ(alone.residuals.size(), 1u);
}

TEST_F(JoinGraphTest, id_is_remapped_to_key) {
  auto q =
      prepare("FOR a IN c1 FOR b IN c2 FILTER a._id == b._id RETURN [a, b]");
  auto g = buildGraph(*q);

  ASSERT_EQ(g.edges.size(), 1u);
  auto const& e = g.edges.front();
  ASSERT_EQ(e.fromAttributes.size(), 1u);
  ASSERT_EQ(e.toAttributes.size(), 1u);
  EXPECT_EQ(e.fromAttributes.front(),
            (AttributePath{std::string_view{"_key"}}));
  EXPECT_EQ(e.toAttributes.front(), (AttributePath{std::string_view{"_key"}}));
}

TEST_F(JoinGraphTest, disconnected_graph_has_two_components) {
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 4u);
  EXPECT_EQ(g.edges.size(), 2u);
  EXPECT_EQ(g.connectedComponents().size(), 2u);
}

TEST_F(JoinGraphTest, single_enumeration_has_no_join) {
  auto q = prepare("FOR a IN c1 FILTER a.x == 1 RETURN a");
  auto g = buildGraph(*q);

  EXPECT_EQ(g.nodes.size(), 1u);
  EXPECT_TRUE(g.edges.empty());
  EXPECT_FALSE(g.hasJoin());
  EXPECT_EQ(g.connectedComponents().size(), 1u);
}

TEST_F(JoinGraphTest, separate_runs_produce_separate_graphs) {
  // The two joins are split by a SORT, which terminates the first run of
  // adjacent enumerations, so the walk produces one graph per run.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "SORT a.x "
      "FOR c IN c3 FOR d IN c1 FILTER c.x == d.y RETURN [a, b, c, d]");
  auto graphs = buildAllGraphs(*q);

  ASSERT_EQ(graphs.size(), 2u);
  for (auto const& g : graphs) {
    EXPECT_EQ(g.nodes.size(), 2u);
    EXPECT_EQ(g.edges.size(), 1u);
    EXPECT_TRUE(g.hasJoin());
    EXPECT_EQ(g.connectedComponents().size(), 1u);
  }
}

TEST_F(JoinGraphTest, adding_an_edge_invalidates_the_adjacency_index) {
  // getEdgesForNode caches an adjacency index of Edge* into the `edges`
  // vector, which appending reallocates. Production never interleaves the two
  // -- the graph is fully built before the search reads it -- so nothing else
  // exercises addJoinCondition's invalidation, and it would rot silently.
  auto q = prepare(
      "FOR a IN c1 FOR b IN c2 FILTER a.x == b.y "
      "FOR c IN c3 FILTER b.z == c.w RETURN [a, b, c]");
  auto g = buildGraph(*q);
  ASSERT_EQ(g.edges.size(), 2u);

  auto* b = nodeByName(g, "b");
  ASSERT_NE(b, nullptr);
  // Populate the cache, and read the count through it.
  auto const before = g.getEdgesForNode(b).size();
  ASSERT_EQ(before, 2u);

  // A fresh edge on the same vertex. Appending reallocates `edges`, so a
  // stale index would hand back dangling pointers or a stale count.
  auto* a = nodeByName(g, "a");
  auto* c = nodeByName(g, "c");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(c, nullptr);
  g.addJoinCondition(a->executionNode->outVariable(), {"p"},
                     c->executionNode->outVariable(), {"q"});
  ASSERT_EQ(g.edges.size(), 3u);

  // b is untouched by the new edge, but its cached Edge* all pointed into the
  // reallocated buffer; the rebuilt index must still report exactly 2, and
  // dereferencing must be safe.
  auto const& after = g.getEdgesForNode(b);
  EXPECT_EQ(after.size(), 2u);
  for (auto const* edge : after) {
    EXPECT_TRUE(edge->from == b || edge->to == b);
  }
  // and a's adjacency must now include the new edge
  EXPECT_EQ(g.getEdgesForNode(a).size(), 2u);
}

TEST_F(JoinGraphTest, non_deterministic_calculation_is_flagged) {
  auto q = prepare(
      "FOR a IN c1 LET r = RAND() FILTER a.x > r "
      "FOR b IN c2 FILTER a.y == b.z RETURN [a, b]");
  auto g = buildGraph(*q);
  EXPECT_TRUE(g.hasNonDeterministicCalculation);
}

TEST_F(JoinGraphTest, deterministic_run_is_not_flagged) {
  auto q = prepare("FOR a IN c1 FOR b IN c2 FILTER a.x == b.y RETURN [a, b]");
  auto g = buildGraph(*q);
  EXPECT_FALSE(g.hasNonDeterministicCalculation);
}

}  // namespace arangodb::tests::aql
