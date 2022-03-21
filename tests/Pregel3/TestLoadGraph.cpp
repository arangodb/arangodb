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

struct TestLoadGraph : ::testing::Test {};

using namespace arangodb::pregel3;

TEST_F(TestLoadGraph, basic) {
  double const capacity = 2.0;
  auto graph = std::make_shared<MinCutGraph>();
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

  auto& flow = cutFlowRes.flow;
  ASSERT_EQ(flow.size(), 1);
  ASSERT_TRUE(flow.find(0) != flow.end());
  ASSERT_EQ(flow[0], capacity);

  auto const& cutEdges = cutFlowRes.cut.edges;
  ASSERT_EQ(cutEdges.size(), 1);
  ASSERT_NE(cutEdges.find(0), cutEdges.end());

  auto const& sourceComp = cutFlowRes.cut.sourceComp;
  ASSERT_EQ(sourceComp.size(), 1);
  ASSERT_NE(sourceComp.find(0), sourceComp.end());
}