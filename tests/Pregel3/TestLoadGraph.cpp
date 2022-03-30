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
/// @author Roman Rabinovich
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "Pregel3/Graph02.h"
#include "Pregel3/MaxFlowMinCut02.h"

struct TestPushRelabel : ::testing::Test {};

using namespace arangodb::pregel3;

TEST_F(TestPushRelabel, oneEdge) {
  double const capacity = 2.0;
  auto graph = std::make_shared<MinCutGraph>();
  // two vertices, one edge
  graph->addVertex();
  graph->addVertex();
  graph->addEdge(0, 1, capacity);
  graph->source = 0;
  graph->target = 1;

  auto algorithm = std::make_unique<MaxFlowMinCut>(
      std::static_pointer_cast<MinCutGraph>(graph),
      dynamic_cast<MinCutGraph*>(graph.get())->source,
      dynamic_cast<MinCutGraph*>(graph.get())->target);

  auto result = algorithm->run();
  ASSERT_NE(result, nullptr);
  auto cutFlowRes = dynamic_cast<MaxFlowMinCutResult&>(*result);

  // test the flow
  auto& flow = cutFlowRes.flow;
  ASSERT_EQ(flow.size(), 1);
  ASSERT_TRUE(flow.find(0) != flow.end());  // edge with idx 0 (from 0 to 1)
  ASSERT_EQ(flow[0], capacity);

  // test the cut
  auto const& cutEdges = cutFlowRes.cut.edges;
  ASSERT_EQ(cutEdges.size(), 1);
  ASSERT_NE(cutEdges.find(0), cutEdges.end());  // edge with idx 0 (from 0 to 1)

  // test the component of the source
  auto const& sourceComp = cutFlowRes.cut.sourceComp;
  ASSERT_EQ(sourceComp.size(), 1);
  ASSERT_NE(sourceComp.find(0), sourceComp.end());  // vertex with idx 0
}

TEST_F(TestPushRelabel, twoStar) {
  double const capacity01 = 2.0;
  double const capacity02 = 3.0;
  auto graph = std::make_shared<MinCutGraph>();
  // source 0 -> 1 target
  //          -> 2
  graph->addVertex();  // 0
  graph->addVertex();  // 1
  graph->addVertex();  // 2
  graph->addEdge(0, 1, capacity01);
  graph->addEdge(0, 2, capacity02);
  graph->source = 0;
  graph->target = 1;

  auto algorithm = std::make_unique<MaxFlowMinCut>(
      std::static_pointer_cast<MinCutGraph>(graph),
      dynamic_cast<MinCutGraph*>(graph.get())->source,
      dynamic_cast<MinCutGraph*>(graph.get())->target);

  auto result = algorithm->run();
  ASSERT_NE(result, nullptr);
  auto cutFlowRes = dynamic_cast<MaxFlowMinCutResult&>(*result);

  // test the flow
  auto& flow = cutFlowRes.flow;
  ASSERT_EQ(flow.size(), 1);
  ASSERT_TRUE(flow.find(0) != flow.end());  // edge with idx 0 (from 0 to 1)
  ASSERT_EQ(flow[0], capacity01);

  // test the cut
  auto const& cutEdges = cutFlowRes.cut.edges;
  ASSERT_EQ(cutEdges.size(), 1);
  ASSERT_NE(cutEdges.find(0), cutEdges.end());  // edge with idx 0 (from 0 to 1)

  // test the component of the source
  auto const& sourceComp = cutFlowRes.cut.sourceComp;
  ASSERT_EQ(sourceComp.size(), 1);
  ASSERT_NE(sourceComp.find(0), sourceComp.end());  // vertex with idx 0
}