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
#include "Basics/ResultT.h"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Queues/LifoQueue.h"

#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace lifo_queue_graph_cache_test {

class Step : public arangodb::graph::BaseStep<Step> {
  size_t _id;
  double _weight;
  bool _isLooseEnd;

 public:
  Step(size_t id, double weight, bool isLooseEnd)
      : arangodb::graph::BaseStep<Step>{} {
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
    return "<Step> _id: " +
           basics::StringUtils::itoa(static_cast<int32_t>(_id)) +
           ", _weight: " + basics::StringUtils::ftoa(_weight);
  }
};

class LifoQueueTest : public ::testing::Test {
  // protected:
 public:
  LifoQueueTest() {}
  ~LifoQueueTest() {}

 public:
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};
};

TEST_F(LifoQueueTest, it_should_be_empty_if_new_queue_initialized) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  ASSERT_EQ(queue.size(), 0);
  ASSERT_TRUE(queue.isEmpty());
}

TEST_F(LifoQueueTest, it_should_contain_element_after_insertion) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  auto step = Step{1, 1, false};
  queue.append(step);
  ASSERT_EQ(queue.size(), 1);
  ASSERT_FALSE(queue.isEmpty());
}

TEST_F(LifoQueueTest, it_should_contain_zero_elements_after_clear) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, false});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  queue.clear();
  ASSERT_TRUE(queue.isEmpty());
}

TEST_F(LifoQueueTest, it_should_contain_processable_elements) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, true});
  queue.append(Step{4, 1, false});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());
}

TEST_F(LifoQueueTest, it_should_not_contain_processable_elements) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, true});
  queue.append(Step{2, 1, true});
  queue.append(Step{3, 1, true});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(LifoQueueTest, it_should_pop_last_element_if_processable) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, true});
  queue.append(Step{2, 1, true});
  queue.append(Step{3, 1, false});
  queue.append(Step{4, 1, false});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());
  while (queue.hasProcessableElement()) {
    std::ignore = queue.pop();
  }
  ASSERT_EQ(queue.size(), 2);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(LifoQueueTest, it_should_pop_in_correct_order) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 1, false});
  queue.append(Step{4, 1, false});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());
  size_t id = 4;
  while (queue.hasProcessableElement()) {
    Step myStep = queue.pop();
    ASSERT_EQ(id, myStep.id());
    id--;
  }
  ASSERT_EQ(queue.size(), 0);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(LifoQueueTest, it_should_pop_all_loose_ends) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, true});
  queue.append(Step{2, 1, true});
  queue.append(Step{3, 1, true});
  queue.append(Step{4, 1, true});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_FALSE(queue.hasProcessableElement());

  std::vector<Step*> myStepReferences = queue.getLooseEnds();
  ASSERT_EQ(myStepReferences.size(), 4);

  size_t id = 4;
  for (auto stepReference : myStepReferences) {
    ASSERT_EQ(stepReference->id(), id);
    id--;
  }

  ASSERT_EQ(queue.size(), 4);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(LifoQueueTest, it_should_allow_to_inject_many_start_vertices) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  std::vector<Step> input;
  input.emplace_back(Step{1, 1, false});
  input.emplace_back(Step{2, 1, false});
  input.emplace_back(Step{3, 1, false});
  input.emplace_back(Step{4, 1, false});
  auto memorySizeBefore = _resourceMonitor.current();
  queue.setStartContent(std::move(input));
  // Account for all 4 added steps.
  EXPECT_EQ(memorySizeBefore + sizeof(Step) * 4, _resourceMonitor.current());
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());

  size_t id = 1;
  // We assume the above ordering to be in LIFO already
  // So do not revert it,but just run from first to last
  while (!queue.isEmpty()) {
    auto step = queue.pop();
    ASSERT_EQ(step.id(), id);
    id++;
  }
  ASSERT_EQ(queue.size(), 0);
  // Memory is reduced fully again
  EXPECT_EQ(memorySizeBefore, _resourceMonitor.current());
}

TEST_F(LifoQueueTest,
       on_many_start_vertices_it_should_handle_appends_correctly) {
  auto queue = LifoQueue<Step>(_resourceMonitor);
  std::vector<Step> input;
  input.emplace_back(Step{1, 1, false});
  // We expand something on 1
  input.emplace_back(Step{6, 1, false});
  input.emplace_back(Step{7, 1, false});
  input.emplace_back(Step{8, 1, false});
  auto memorySizeBefore = _resourceMonitor.current();
  queue.setStartContent(std::move(input));
  // Account for all 4 added steps.
  EXPECT_EQ(memorySizeBefore + sizeof(Step) * 4, _resourceMonitor.current());
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());

  size_t id = 1;
  {
    // Pop First entry, add two more new ones
    auto step = queue.pop();
    ASSERT_EQ(step.id(), id);
    id++;
    queue.append(Step{5, 1, false});
    // We expand some on 2
    queue.append(Step{2, 1, false});
  }
  {
    // Pop Second entry, add two more new ones
    auto step = queue.pop();
    ASSERT_EQ(step.id(), id);
    id++;
    queue.append(Step{4, 1, false});
    queue.append(Step{3, 1, false});
  }
  // Ids are increasing in order of FIFO sorting.
  // so lets now pull everything from queue in expected order
  ASSERT_EQ(queue.size(), 6);
  while (!queue.isEmpty()) {
    auto step = queue.pop();
    ASSERT_EQ(step.id(), id);
    id++;
  }
  ASSERT_EQ(queue.size(), 0);
  // Memory is reduced fully again
  EXPECT_EQ(memorySizeBefore, _resourceMonitor.current());
}

}  // namespace lifo_queue_graph_cache_test
}  // namespace tests
}  // namespace arangodb
