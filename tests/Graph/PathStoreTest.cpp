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

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StringUtils.h"
#include "Graph/PathManagement/PathStore.cpp"
#include "Graph/Providers/BaseStep.h"

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

class PathStoreTest : public ::testing::Test {
 protected:
  PathStoreTest() {}
  ~PathStoreTest() {}

  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};

  PathStore<Step> testee() { return PathStore<Step>(_resourceMonitor); }
};

TEST_F(PathStoreTest, it_should_be_empty_if_new_path_store_is_initialized) {
  auto ps = testee();
  EXPECT_EQ(ps.size(), 0);
}

TEST_F(PathStoreTest, it_should_be_able_to_set_startVertex) {
  auto ps = testee();
  EXPECT_EQ(ps.size(), 0);
  std::ignore = ps.append({0, 1, 0, false});
  EXPECT_EQ(ps.size(), 1);
}

TEST_F(PathStoreTest, it_should_be_able_to_clear) {
  auto ps = testee();
  size_t memoryUsage = _resourceMonitor.current();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.current(), memoryUsage);
  memoryUsage = _resourceMonitor.current();
  lastIndex = ps.append({1, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.current(), memoryUsage);
  memoryUsage = _resourceMonitor.current();
  lastIndex = ps.append({2, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.current(), memoryUsage);
  memoryUsage = _resourceMonitor.current();
  lastIndex = ps.append({3, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.current(), memoryUsage);
  memoryUsage = _resourceMonitor.current();
  lastIndex = ps.append({4, 1, lastIndex, false});
  EXPECT_GT(_resourceMonitor.current(), memoryUsage);
  memoryUsage = _resourceMonitor.current();
  EXPECT_EQ(ps.size(), 5);

  ps.reset();
  memoryUsage = _resourceMonitor.current();

  EXPECT_EQ(ps.size(), 0);
  EXPECT_EQ(_resourceMonitor.current(), memoryUsage);
  EXPECT_EQ(_resourceMonitor.current(), 0);
}

TEST_F(PathStoreTest, it_should_be_able_to_append_on_empty_clear_and_reappend) {
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

TEST_F(PathStoreTest, it_should_not_be_empty_if_values_will_be_inserted) {
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

TEST_F(PathStoreTest, it_should_provide_a_path_visitor) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  // Start at Id 1 to simplify test loop
  lastIndex = ps.append({1, 1, lastIndex, false});

  lastIndex = ps.append({2, 1, lastIndex, false});

  lastIndex = ps.append({3, 1, lastIndex, false});

  Step last{4, 1, lastIndex, false};
  ps.append(last);

  EXPECT_EQ(ps.size(), 4);
  size_t expectedId = 4;
  auto visitor = [&](Step const& step) -> bool {
    EXPECT_EQ(expectedId, step.getVertex());
    expectedId--;
    return true;
  };
  ps.visitReversePath(last, visitor);
  // We started at 1 so, we need to end up at expected == 0
  EXPECT_EQ(expectedId, 0);
}

TEST_F(PathStoreTest, it_should_abort_a_path_visitor_if_it_returns_false) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  // Start at Id 1 to simplify test loop
  lastIndex = ps.append({1, 1, lastIndex, false});

  lastIndex = ps.append({2, 1, lastIndex, false});

  lastIndex = ps.append({3, 1, lastIndex, false});

  Step last{4, 1, lastIndex, false};
  ps.append(last);

  EXPECT_EQ(ps.size(), 4);
  size_t expectedId = 4;
  auto visitor = [&](Step const& step) -> bool {
    EXPECT_EQ(expectedId, step.getVertex());
    expectedId--;
    if (expectedId == 2) {
      return false;
    }
    return true;
  };
  ps.visitReversePath(last, visitor);
  // We aborted at 2 so, we need to end up at expected == 2
  EXPECT_EQ(expectedId, 2);
}

TEST_F(PathStoreTest, it_should_only_visit_one_path) {
  auto ps = testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  // Start at Id 1 to simplify test loop
  lastIndex = ps.append({1, 1, lastIndex, false});

  // Add some noise
  ps.append({41, 1, lastIndex, false});
  ps.append({42, 1, lastIndex, false});

  lastIndex = ps.append({2, 1, lastIndex, false});

  // Add some noise
  ps.append({43, 1, lastIndex, false});
  ps.append({44, 1, lastIndex, false});

  lastIndex = ps.append({3, 1, lastIndex, false});

  // Add some noise
  ps.append({45, 1, lastIndex, false});
  ps.append({46, 1, lastIndex, false});

  Step last{4, 1, lastIndex, false};
  lastIndex = ps.append(last);

  // Add some noise
  ps.append({47, 1, lastIndex, false});
  ps.append({48, 1, lastIndex, false});

  // 4 on path, + 8 times noise.
  EXPECT_EQ(ps.size(), 12);
  size_t expectedId = 4;
  auto visitor = [&](Step const& step) -> bool {
    EXPECT_EQ(expectedId, step.getVertex());
    expectedId--;
    return true;
  };
  ps.visitReversePath(last, visitor);
  // We started at 1 so, we need to end up at expected == 0
  EXPECT_EQ(expectedId, 0);
}

}  // namespace graph_path_store_test
}  // namespace tests
}  // namespace arangodb
