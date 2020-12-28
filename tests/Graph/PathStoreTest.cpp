////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Graph/PathManagement/PathStore.cpp"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Types/UniquenessLevel.h"

#include <Basics/StringUtils.h>
#include <ostream>

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace graph_path_store_test {

class Step : public arangodb::graph::BaseStep<Step> {
  using Vertex = size_t;
  using Edge = size_t;

  size_t _id;
  double _weight;
  bool _isLooseEnd;

 public:
  Step(size_t id, double weight, size_t previous, bool isLooseEnd) : arangodb::graph::BaseStep<Step>{previous} {
    _id = id;
    _weight = weight;
    _isLooseEnd = isLooseEnd;  // TODO: needed here?
  };

  ~Step() = default;

  Step(Step const& other) = default;
  Step& operator=(Step const& other) = delete;
  bool operator==(Step const& other) { return _id == other._id; }
  std::string toString() const {
    return "<Step> _id: " + basics::StringUtils::itoa(_id) +
           ", _weight: " + basics::StringUtils::ftoa(_weight) +
           ", _previous: " + basics::StringUtils::itoa(getPrevious());
  }

  bool isProcessable() const { return _isLooseEnd ? false : true; }
  size_t getVertex() const { return _id; }  // TODO: adjust
  size_t getEdge() const { return _id; }    // TODO: adjust
};

class PathStoreTest : public ::testing::TestWithParam<VertexUniquenessLevel> {
 protected:
  PathStoreTest() {}
  ~PathStoreTest() {}

  arangodb::ResourceMonitor _resourceMonitor{};

  PathStore<Step> testee() { return PathStore<Step>(_resourceMonitor); }

  VertexUniquenessLevel getVertexUniquness() { return GetParam(); }
};

TEST_P(PathStoreTest, it_should_be_empty_if_new_path_store_is_initialized) {
  auto ps = testee();
  EXPECT_EQ(ps.size(), 0);
}

TEST_P(PathStoreTest, it_should_be_able_to_set_startVertex) {
  auto ps = testee();
  EXPECT_EQ(ps.size(), 0);
  std::ignore = ps.append({0, 1, 0, false});
  EXPECT_EQ(ps.size(), 1);
}

TEST_P(PathStoreTest, it_should_be_able_to_clear) {
  auto ps = testee();
  size_t memoryUsage = _resourceMonitor.currentMemoryUsage();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.currentMemoryUsage(), memoryUsage);
  memoryUsage = _resourceMonitor.currentMemoryUsage();
  lastIndex = ps.append({1, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.currentMemoryUsage(), memoryUsage);
  memoryUsage = _resourceMonitor.currentMemoryUsage();
  lastIndex = ps.append({2, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.currentMemoryUsage(), memoryUsage);
  memoryUsage = _resourceMonitor.currentMemoryUsage();
  lastIndex = ps.append({3, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.currentMemoryUsage(), memoryUsage);
  memoryUsage = _resourceMonitor.currentMemoryUsage();
  lastIndex = ps.append({4, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.currentMemoryUsage(), memoryUsage);
  memoryUsage = _resourceMonitor.currentMemoryUsage();
  EXPECT_EQ(ps.size(), 5);

  ps.reset();
  memoryUsage = _resourceMonitor.currentMemoryUsage();

  EXPECT_EQ(ps.size(), 0);
  EXPECT_EQ(_resourceMonitor.currentMemoryUsage(), memoryUsage);
  EXPECT_EQ(_resourceMonitor.currentMemoryUsage(), 0);
}

TEST_P(PathStoreTest, it_should_be_able_to_append_on_empty_clear_and_reappend) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  lastIndex = ps.append({1, 1, lastIndex, false});
  lastIndex = ps.append({2, 1, lastIndex, false});
  lastIndex = ps.append({3, 1, lastIndex, false});
  lastIndex = ps.append({4, 1, lastIndex, false});
  EXPECT_EQ(ps.size(), 5);
  ps.reset();

  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(ps.size(), 1);
}

TEST_P(PathStoreTest, it_should_not_be_empty_if_values_will_be_inserted) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);

  lastIndex = ps.append({1, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 1);

  lastIndex = ps.append({2, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 2);

  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 3);

  EXPECT_EQ(ps.size(), 4);
}

TEST_P(PathStoreTest, it_should_honor_uniqueness_on_single_path_first_duplicate) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add a loop that ends in the start vertex (0) again.
  {
    Step s{1, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }
  
  // Add duplicate vertex on Path
  {
    Step s{0, 1, lastIndex, false};
    auto res = ps.testPath(s);
    if (getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TEST_P(PathStoreTest, it_should_honor_uniqueness_on_single_path_last_duplicate) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add a loop on the last vertex of the path (3)
  {
    Step s{1, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }
  
  // Add duplicate vertex on Path
  {
    Step s{3, 1, lastIndex, false};
    auto res = ps.testPath(s);
    if (getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}


TEST_P(PathStoreTest, it_should_honor_uniqueness_on_single_path_interior_duplicate) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add a loop that ends in one interior vertex (2) again.
  {
    Step s{1, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }
  
  // Add duplicate vertex on Path
  {
    Step s{2, 1, lastIndex, false};
    auto res = ps.testPath(s);
    if (getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TEST_P(PathStoreTest, it_should_honor_uniqueness_on_global_paths_last_duplicate) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s{1, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }
  
  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s{4, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s{5, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }
  
  // Add duplicate vertex (3) which is the last on the first path
  {
    Step s{3, 1, lastIndex, false};
    auto res = ps.testPath(s);
    if (getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
      // The vertex is visited twice, but not on same path.
      // As long as we are not GLOBAL this is okay.
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TEST_P(PathStoreTest, it_should_honor_uniqueness_on_global_paths_interior_duplicate) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s{1, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }
  
  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s{4, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s{5, 1, lastIndex, false};
    auto res = ps.testPath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }
  
  // Add duplicate vertex (1) which is interior to the first path
  {
    Step s{1, 1, lastIndex, false};
    auto res = ps.testPath(s);
    if (getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
      // The vertex is visited twice, but not on same path.
      // As long as we are not GLOBAL this is okay.
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}



INSTANTIATE_TEST_CASE_P(PathStoreTestSuite, PathStoreTest,
                        ::testing::Values(VertexUniquenessLevel::NONE,
                                          VertexUniquenessLevel::PATH,
                                          VertexUniquenessLevel::GLOBAL));

}  // namespace graph_path_store_test
}  // namespace tests
}  // namespace arangodb
