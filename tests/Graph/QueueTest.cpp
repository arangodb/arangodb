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

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StringUtils.h"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Queues/FifoQueue.h"

#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace queue_graph_cache_test {

class Step : public arangodb::graph::BaseStep<Step> {
  size_t _id;
  double _weight;
  bool _isLooseEnd;

 public:
  Step(size_t id, double weight, bool isLooseEnd) : arangodb::graph::BaseStep<Step>{} {
    _id = id;
    _weight = weight;
    _isLooseEnd = isLooseEnd;
  };

  ~Step() = default;

  Step(Step const& other) = default;
  Step& operator=(Step const& other) = delete;
  bool operator==(Step const& other) { return _id == other._id; }

  bool isProcessable() const { return _isLooseEnd ? false : true; }
  size_t id() const { return _id; }
  std::string toString() {
    return "<Step> _id: " + basics::StringUtils::itoa(static_cast<int32_t>(_id)) +
           ", _weight: " + basics::StringUtils::ftoa(_weight); // TODO: Add isLooseEnd
  }
};

class QueueTest : public ::testing::Test {
 //protected:
 public:
  QueueTest() {}
  ~QueueTest() {}

 public:
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};
};

TEST_F(QueueTest, it_should_be_empty_if_new_queue_initialized) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  ASSERT_EQ(queue.size(), 0);
  ASSERT_TRUE(queue.isEmpty());
}

TEST_F(QueueTest, it_should_contain_element_after_insertion) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  auto step = Step{1, 1, false};
  queue.append(step);
  ASSERT_EQ(queue.size(), 1);
  ASSERT_FALSE(queue.isEmpty());
}

TEST_F(QueueTest, it_should_contain_zero_elements_after_clear) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, false});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  queue.clear();
  ASSERT_TRUE(queue.isEmpty());
}

TEST_F(QueueTest, it_should_contain_processable_elements) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, false});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());
}

TEST_F(QueueTest, it_should_not_contain_processable_elements) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, true});
  queue.append(Step{2, 1, true});
  queue.append(Step{3, 1, true});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(QueueTest, it_should_pop_first_element_if_processable) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, true});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());
  while (queue.hasProcessableElement()) {
    std::ignore = queue.pop();
  }
  ASSERT_EQ(queue.size(), 2);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(QueueTest, it_should_pop_in_correct_order) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, false});
  queue.append(Step{4, 1, false});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());
  size_t id = 1;
  while (queue.hasProcessableElement()) {
    Step myStep = queue.pop();
    ASSERT_EQ(id, myStep.id());
    id++;
  }
  ASSERT_EQ(queue.size(), 0);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(QueueTest, it_should_pop_all_loose_ends) {
  auto queue = FifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, true});
  queue.append(Step{2, 1, true});
  queue.append(Step{3, 1, true});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_FALSE(queue.hasProcessableElement());

  std::vector<Step*> myStepReferences = queue.getLooseEnds();
  ASSERT_EQ(myStepReferences.size(), 4);

  size_t id = 1;
  for (auto stepReference : myStepReferences) {
    ASSERT_EQ(stepReference->id(), id);
    id++;
  }

  ASSERT_EQ(queue.size(), 4);
  ASSERT_FALSE(queue.hasProcessableElement());
}

}  // namespace queue_graph_cache_test
}  // namespace tests
}  // namespace arangodb
