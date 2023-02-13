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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Inspection/Format.h"
#include "Inspection/JsonPrintInspector.h"

#include "Pregel/GraphStore/Vertex.h"
#include "Pregel/GraphStore/Quiver.h"

using namespace arangodb;
using namespace arangodb::pregel;

using MyQuiver = Quiver<std::string, std::string>;

TEST(PregelQuiver, vertex_construction) {
  auto store = MyQuiver{};

  auto v = MyQuiver::VertexType();
  ASSERT_TRUE(v.active());
  ASSERT_EQ(v.getEdgeCount(), 0);
  ASSERT_EQ(v.pregelId(), VertexID());
}

TEST(PregelQuiver, vertex_and_edges) {
  using MyQuiver = Quiver<std::string, std::string>;
  auto store = MyQuiver{};

  auto v = MyQuiver::VertexType();

  auto data = std::vector<std::pair<VertexID, std::string>>{
      {{PregelShard(5), "foo"}, "data"}, {{PregelShard(6), "bar"}, "moredata"}};

  ASSERT_EQ(v.getEdgeCount(), 0);
  for (auto d : data) {
    auto edgeCount = v.getEdgeCount();
    auto e = MyQuiver::EdgeType(d.first, std::move(d.second));
    v.addEdge(std::move(e));
    ASSERT_EQ(v.getEdgeCount(), edgeCount + 1);
  }

  ASSERT_EQ(v.getEdgeCount(), data.size());

  auto& edges = v.getEdges();
  for (size_t i = 0; i < data.size(); ++i) {
    ASSERT_EQ(edges[i]._to, data[i].first);
    ASSERT_EQ(edges[i].data(), data[i].second);
  }
}

TEST(PregelQuiver, storing_some_vertices) {
  using MyQuiver = Quiver<std::string, std::string>;
  auto store = MyQuiver{};

  for (size_t i = 0; i < 155; ++i) {
    auto v = MyQuiver::VertexType();
    store.vertices.emplace_back(std::move(v));
  }

  ASSERT_EQ(store.numberOfVertices(), 155);
}

// TODO: A test that creates a number of vertices with edges
