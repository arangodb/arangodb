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
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <variant>
#include <fstream>
#include "Graph.h"
#include "GraphsSource.h"
#include "fmt/format.h"
#include "WCCGraph.h"

using namespace arangodb::slicegraph;

namespace {
auto testReadFromFile(SharedSlice const& graphSlice,
                      bool checkDuplicateVertices, bool checkEdgeEnds) {
  size_t expectedNumVertices = GraphSliceHelper::getNumVertices(graphSlice);
  size_t expectedNumEdges = GraphSliceHelper::getNumEdges(graphSlice);
  std::string const vertexFileName = "skdjf444bvl4dkj456fbv4r43k";
  std::string const edgeFileName = "kdjfljdkjcdkjdkjoki349023kd";
  {
    std::ofstream vertexFile(vertexFileName);
    vertexFile.clear();
    auto const vs = graphSlice["vertices"];
    for (size_t i = 0; i < vs.length(); ++i) {
      vertexFile << vs[i].toJson() << '\n';
    }
    vertexFile.close();
  }

  {
    std::ofstream edgeFile(edgeFileName);
    edgeFile.clear();
    auto const es = graphSlice["edges"];
    for (size_t i = 0; i < es.length(); ++i) {
      edgeFile << es[i].toJson() << '\n';
    }
    edgeFile.close();
  }
  Graph<EmptyEdgeProperties, WCCVertexProperties> graph{};
  std::ifstream vertexFile(vertexFileName);
  graph.readVertices(vertexFile, checkDuplicateVertices);
  EXPECT_EQ(graph.vertices.size(), expectedNumVertices);

  std::ifstream edgeFile(edgeFileName);
  size_t numEdges = graph.readEdges(
      edgeFile, checkEdgeEnds,
      [&](const Graph<EmptyEdgeProperties, WCCVertexProperties>::Edge&) {});
  EXPECT_EQ(numEdges, expectedNumEdges);
  try {
    std::remove(vertexFileName.c_str());
    std::remove(edgeFileName.c_str());
  } catch (...) {
    std::cerr << fmt::format("Could not remove files {} and {}.",
                             vertexFileName, edgeFileName);
  }
}
}  // namespace

TEST(GWEN_GRAPH, test_graph_read_from_file_2path) {
  bool checkDuplicateVertices = true;
  bool checkEdgeEnds = true;
  auto graphSlice = arangodb::slicegraph::setup2Path();
  testReadFromFile(graphSlice, checkDuplicateVertices, checkEdgeEnds);
}

TEST(GWEN_GRAPH, test_graph_read_from_file_alternatingTree) {
  bool checkDuplicateVertices = true;
  bool checkEdgeEnds = true;
  auto graphSlice = setup1AlternatingTree();
  testReadFromFile(graphSlice, checkDuplicateVertices, checkEdgeEnds);
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
