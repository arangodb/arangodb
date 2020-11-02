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

#include <Basics/StringUtils.h>
#include <ostream>

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace graph_path_store_test {

class Step {
  using Vertex = size_t;
  using Edge = size_t;

  size_t _id;
  double _weight;
  size_t _previous;
  bool _isLooseEnd;

 public:
  Step(size_t id, double weight, size_t previous, bool isLooseEnd) {
    _id = id;
    _weight = weight;
    _previous = previous;
    _isLooseEnd = isLooseEnd;  // TODO: needed here?
  };

  ~Step() = default;

  Step(Step const& other) = default;
  Step& operator=(Step const& other) = delete;
  bool operator==(Step const& other) { return _id == other._id; }
  std::string toString() const {
    return "<Step> _id: " + basics::StringUtils::itoa(_id) +
           ", _weight: " + basics::StringUtils::ftoa(_weight) +
           ", _previous: " + basics::StringUtils::itoa(_previous);
  }

  bool isProcessable() const { return _isLooseEnd ? false : true; }
  bool isFirst() const {
    return _previous == std::numeric_limits<size_t>::max();
  }
  size_t getPrevious() const { return _previous; }
  size_t getVertex() const { return _id; }  // TODO: adjust
  size_t getEdge() const { return _id; }    // TODO: adjust
};

class PathStoreTest : public ::testing::Test {
 protected:
  PathStoreTest() {}
  ~PathStoreTest() {}
};

TEST_F(PathStoreTest, it_should_be_empty_if_new_path_store_is_initialized) {
  auto ps = PathStore<Step>();
  ASSERT_EQ(ps.size(), 0);
}

TEST_F(PathStoreTest, it_should_be_able_to_set_startVertex) {
  auto ps = PathStore<Step>();
  ASSERT_EQ(ps.size(), 0);
  std::ignore = ps.append({0, 1, 0, false});
  ASSERT_EQ(ps.size(), 1);
}

TEST_F(PathStoreTest, it_should_be_able_to_clear) {
  auto ps = PathStore<Step>();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  lastIndex = ps.append({1, 1, lastIndex, false});
  lastIndex = ps.append({2, 1, lastIndex, false});
  lastIndex = ps.append({3, 1, lastIndex, false});
  lastIndex = ps.append({4, 1, lastIndex, false});
  ASSERT_EQ(ps.size(), 5);
  ps.reset();
  ASSERT_EQ(ps.size(), 0);
}

TEST_F(PathStoreTest, it_should_be_able_to_append_on_empty_clear_and_reappend) {
  auto ps = PathStore<Step>();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  lastIndex = ps.append({1, 1, lastIndex, false});
  lastIndex = ps.append({2, 1, lastIndex, false});
  lastIndex = ps.append({3, 1, lastIndex, false});
  lastIndex = ps.append({4, 1, lastIndex, false});
  ASSERT_EQ(ps.size(), 5);
  ps.reset();

  lastIndex = ps.append({0, 1, lastIndex, false});
  ASSERT_EQ(ps.size(), 1);
}

TEST_F(PathStoreTest, it_should_not_be_empty_if_values_will_be_inserted) {
  auto ps = PathStore<Step>();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  ASSERT_EQ(lastIndex, 0);

  lastIndex = ps.append({1, 1, lastIndex, false});
  ASSERT_EQ(lastIndex, 1);

  lastIndex = ps.append({2, 1, lastIndex, false});
  ASSERT_EQ(lastIndex, 2);

  lastIndex = ps.append({0, 1, lastIndex, false});
  ASSERT_EQ(lastIndex, 3);

  ASSERT_EQ(ps.size(), 4);
}

}  // namespace graph_path_store_test
}  // namespace tests
}  // namespace arangodb
