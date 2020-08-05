////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "GraphTestTools.h"

#include "Basics/StringUtils.h"
#include "Graph/KPathFinder.h"
#include "Graph/ShortestPathOptions.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

class KPathFinderTest : public ::testing::Test {
 protected:
  GraphTestSetup s;
  MockGraphDatabase gdb;

  std::unique_ptr<arangodb::aql::Query> query;
  std::unique_ptr<arangodb::graph::ShortestPathOptions> spo;

  std::unique_ptr<KPathFinder> finder;

  KPathFinderTest() : gdb(s.server, "testVocbase") {
    gdb.addVertexCollection("v", 100);
    gdb.addEdgeCollection(
        "e", "v",
        {
          /* a chain 1->2->3->4 */
          {1, 2},   {2, 3},   {3, 4},
          /* a diamond 5->6|7|8->9 */
          {5, 6}, {5,7}, {5,8}, {6,9}, {7,9}, {8,9}
          });

    query = gdb.getQuery("RETURN 1", std::vector<std::string>{"v", "e"});
    spo = gdb.getShortestPathOptions(query.get());

    finder = std::make_unique<KPathFinder>(*spo);
  }

  auto vId(size_t nr) -> std::string {
    return "v/" + basics::StringUtils::itoa(nr);
  }

  auto pathStructureValid(VPackSlice path, size_t depth) -> void {
    ASSERT_TRUE(path.isObject());
    {
      // Check Vertices
      ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryVertices));
      auto vertices = path.get(StaticStrings::GraphQueryVertices);
      ASSERT_TRUE(vertices.isArray());
      ASSERT_EQ(vertices.length(), depth + 1);
      for (auto const& v : VPackArrayIterator(vertices)) {
        EXPECT_TRUE(v.isObject());
      }
    }
    {
      // Check Edges
      ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryEdges));
      auto edges = path.get(StaticStrings::GraphQueryEdges);
      ASSERT_TRUE(edges.isArray());
      ASSERT_EQ(edges.length(), depth);
      for (auto const& e : VPackArrayIterator(edges)) {
        EXPECT_TRUE(e.isObject());
      }
    }
  }

  auto verticesToString(VPackSlice path) -> std::string {
    TRI_ASSERT(path.isObject());
    TRI_ASSERT(path.hasKey(StaticStrings::GraphQueryVertices));
    auto vertices = path.get(StaticStrings::GraphQueryVertices);

    std::string res;
    for (auto const& v : VPackArrayIterator(vertices)) {
      res += v.get(StaticStrings::KeyString).copyString();
    }
    return res;
  }

  auto edgesToString(VPackSlice path) -> std::string {
    TRI_ASSERT(path.isObject());
    TRI_ASSERT(path.hasKey(StaticStrings::GraphQueryEdges));
    auto edges = path.get(StaticStrings::GraphQueryEdges);

    std::string res;
    for (auto const& e : VPackArrayIterator(edges)) {
      res += e.get(StaticStrings::KeyString).copyString();
    }
    return res;
  }
};

TEST_F(KPathFinderTest, no_path_exists) {
  VPackBuilder result;
  // No path between those
  auto source = vId(91);
  auto target = vId(99);
  finder->reset(StringRef(source), StringRef(target));

  EXPECT_TRUE(finder->hasMore());
  {
    result.clear();
    auto hasPath = finder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(finder->hasMore());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(finder->hasMore());
  }
}

TEST_F(KPathFinderTest, path_depth_0) {
  VPackBuilder result;
  // Search 0 depth
  spo->minDepth = 0;
  spo->maxDepth = 0;

  // Source and target identical
  auto source = vId(91);
  auto target = vId(91);

  finder->reset(StringRef(source), StringRef(target));

  EXPECT_TRUE(finder->hasMore());
  {
    result.clear();
    auto hasPath = finder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 0);
    LOG_DEVEL << result.toString();

    EXPECT_TRUE(finder->hasMore());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(finder->hasMore());
  }
}

TEST_F(KPathFinderTest, path_depth_1) {
  VPackBuilder result;
  // Search 0 depth
  spo->minDepth = 1;
  spo->maxDepth = 1;

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(2);

  finder->reset(StringRef(source), StringRef(target));

  EXPECT_TRUE(finder->hasMore());
  {
    result.clear();
    auto hasPath = finder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    LOG_DEVEL << result.toString();

    EXPECT_TRUE(finder->hasMore());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(finder->hasMore());
  }
}

TEST_F(KPathFinderTest, path_depth_2) {
  VPackBuilder result;
  // Search 0 depth
  spo->minDepth = 2;
  spo->maxDepth = 2;

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(3);

  finder->reset(StringRef(source), StringRef(target));

  EXPECT_TRUE(finder->hasMore());
  {
    result.clear();
    auto hasPath = finder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    LOG_DEVEL << result.toString();

    EXPECT_TRUE(finder->hasMore());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(finder->hasMore());
  }
}

TEST_F(KPathFinderTest, path_depth_3) {
  VPackBuilder result;
  // Search 0 depth
  spo->minDepth = 3;
  spo->maxDepth = 3;

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(3);

  finder->reset(StringRef(source), StringRef(target));

  EXPECT_TRUE(finder->hasMore());
  {
    result.clear();
    auto hasPath = finder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 3);
    LOG_DEVEL << result.toString();

    EXPECT_TRUE(finder->hasMore());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(finder->hasMore());
  }
}

TEST_F(KPathFinderTest, path_depth_1_to_2) {
  VPackBuilder result;
  // Search 0 depth
  spo->minDepth = 1;
  spo->maxDepth = 2;

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(3);

  finder->reset(StringRef(source), StringRef(target));

  EXPECT_TRUE(finder->hasMore());
  {
    result.clear();
    auto hasPath = finder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    LOG_DEVEL << result.toString();

    EXPECT_TRUE(finder->hasMore());
  }

  {
    result.clear();
    auto hasPath = finder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    LOG_DEVEL << result.toString();

    EXPECT_TRUE(finder->hasMore());
  }


  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(finder->hasMore());
  }
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb