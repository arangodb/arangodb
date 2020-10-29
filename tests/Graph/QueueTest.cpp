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

#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/QueryWarnings.h"
#include "Cluster/ServerState.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/GraphTestTools.h"
#include "Graph/Queues/FifoQueue.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace queue_graph_cache_test {

class Step {
  size_t _id;
  double _weight;
  bool _isLooseEnd;

 public:
  Step(size_t id, double weight, bool isLooseEnd) {
    _id = id;
    _weight = weight;
    _isLooseEnd = isLooseEnd;
  };

  ~Step() = default;

  Step(Step const& other) = default;
  Step& operator=(Step const& other) = delete;
  bool operator==(Step const& other) { return _id == other._id; }

  bool isProcessable() { return _isLooseEnd ? false : true; }
};

class QueueTest : public ::testing::Test {
 protected:
  QueueTest() {}
  ~QueueTest() {}
};

TEST_F(QueueTest, it_should_be_empty_if_new_queue_initialized) {
  auto queue = FifoQueue<Step>();
  ASSERT_TRUE(queue.size() == 0);
  ASSERT_TRUE(queue.isEmpty());
}

TEST_F(QueueTest, it_should_contain_element_after_insertion) {
  auto queue = FifoQueue<Step>();
  auto step = Step{1, 1, false};
  queue.append(step);
  ASSERT_TRUE(queue.size() == 1);
  ASSERT_FALSE(queue.isEmpty());
}

TEST_F(QueueTest, it_should_contain_processable_elements) {
  auto queue = FifoQueue<Step>();
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, false});
  queue.append(Step{4, 1, true});
  ASSERT_TRUE(queue.size() == 4);
  ASSERT_TRUE(queue.hasProcessableVertex());
}

TEST_F(QueueTest, it_should_not_contain_processable_elements) {
  auto queue = FifoQueue<Step>();
  queue.append(Step{1, 1, true});
  queue.append(Step{2, 1, true});
  queue.append(Step{3, 1, true});
  queue.append(Step{4, 1, true});
  ASSERT_TRUE(queue.size() == 4);
  ASSERT_FALSE(queue.hasProcessableVertex());
}

}  // namespace queue_graph_cache_test
}  // namespace tests
}  // namespace arangodb
