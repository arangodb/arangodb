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
#include "Basics/ResultT.h"
#include "Basics/StringUtils.h"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Queues/WeightedQueue.h"

#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace queue_graph_cache_test {

class Step : public arangodb::graph::BaseStep<Step> {
  size_t _id;
  bool _isLooseEnd;

 public:
  Step(size_t id, double weight, bool isLooseEnd)
      : arangodb::graph::BaseStep<Step>{0, 1, weight} {
    _id = id;
    _isLooseEnd = isLooseEnd;
  };

  ~Step() = default;

  Step(Step const& other) = default;
  Step& operator=(Step const& other) = delete;
  Step(Step&& other) = default;
  Step& operator=(Step&& other) = default;

  bool operator==(Step const& other) { return _id == other._id; }

  bool isProcessable() const { return _isLooseEnd ? false : true; }
  size_t id() const { return _id; }
  std::string toString() {
    return "<Step> _id: " + basics::StringUtils::itoa(static_cast<int32_t>(_id)) +
           ", _weight: " + basics::StringUtils::ftoa(getWeight());
  }
};

class WeightedQueueTest : public ::testing::Test {
  // protected:
 public:
  WeightedQueueTest() {}
  ~WeightedQueueTest() {}

 public:
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};
};

TEST_F(WeightedQueueTest, it_should_be_empty_if_new_queue_initialized) {
  auto queue = WeightedQueue<Step>(_resourceMonitor);
  ASSERT_EQ(queue.size(), 0);
  ASSERT_TRUE(queue.isEmpty());
}

TEST_F(WeightedQueueTest, it_should_contain_element_after_insertion) {
  auto queue = WeightedQueue<Step>(_resourceMonitor);
  auto step = Step{1, 1, false};
  queue.append(step);
  ASSERT_EQ(queue.size(), 1);
  ASSERT_FALSE(queue.isEmpty());
}

TEST_F(WeightedQueueTest, it_should_contain_zero_elements_after_clear) {
  auto queue = WeightedQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 1, false});
  queue.append(Step{2, 4, false});
  queue.append(Step{3, 2, false});
  queue.append(Step{4, 3, true});
  ASSERT_EQ(queue.size(), 4);
  queue.clear();
  ASSERT_TRUE(queue.isEmpty());
}

TEST_F(WeightedQueueTest, it_should_contain_processable_elements) {
  auto queue = WeightedQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 5, false});
  queue.append(Step{2, 1, false});
  queue.append(Step{3, 2, true});
  queue.append(Step{4, 1.6, false});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_TRUE(queue.hasProcessableElement());
}

TEST_F(WeightedQueueTest, it_should_not_contain_processable_elements) {
  auto queue = WeightedQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 4, true});
  queue.append(Step{2, 1.6, true});
  queue.append(Step{3, 1.2, true});
  queue.append(Step{4, 1.5, true});
  ASSERT_EQ(queue.size(), 4);
  ASSERT_FALSE(queue.hasProcessableElement());
}

TEST_F(WeightedQueueTest, it_should_prioritize_processable_elements) {
  // 2 and 3 have identical and smallest weight.
  // 3 is processable, 2 not.
  auto queue = WeightedQueue<Step>(_resourceMonitor);
  queue.append(Step{1, 8, true});
  queue.append(Step{2, 2, true});
  queue.append(Step{3, 2, false});
  queue.append(Step{4, 6, false});
  EXPECT_EQ(queue.size(), 4);
  EXPECT_TRUE(queue.hasProcessableElement());
  auto s = queue.pop();
  EXPECT_EQ(s.id(), 3);
  EXPECT_FALSE(queue.hasProcessableElement());
  EXPECT_EQ(queue.size(), 3);
}

TEST_F(WeightedQueueTest, it_should_order_by_asc_weight) {
  // Random Input in random order. We shuffle it before each iteration, feel
  // free to modify this in any way you like
  std::vector<Step> input{{1, 1, false}, {2, 4, false}, {3, 6, false}, {4, 12, false}};
  // Some test orderings, feel free to add more orderings for tests.
  std::vector<std::pair<std::string, std::function<bool(Step const&, Step const&)>>> orderings{
      {"DescWeight",
       [](Step const& a, Step const& b) -> bool {
         // DESC weight
         return a.getWeight() > b.getWeight();
       }},
      {"AscWeight",
       [](Step const& a, Step const& b) -> bool {
         // ASC weight
         return a.getWeight() < b.getWeight();
       }},
      {"RandomOrder", [](Step const& a, Step const& b) -> bool {
         // RandomWeightOrder, first inject all uneven Steps, sort each "package" by ASC weight
         // There is no specicial plan behind this, it is to stable "non"-sort by weight
         auto modA = a.id() % 2;
         auto modB = a.id() % 2;
         if (modA != modB) {
           return modA > modB;
         }
         return a.getWeight() < b.getWeight();
       }}};
  // No matter how the input is ordered,
  // We need to get it back in exactly the same order, by asc weight
  for (auto [name, o] : orderings) {
    SCOPED_TRACE("Input ordered by " + name);
    std::sort(input.begin(), input.end(), o);
    auto queue = WeightedQueue<Step>(_resourceMonitor);
    for (auto s : input) {
      queue.append(s);
    }
    // We start with all inputs injected.
    EXPECT_EQ(queue.size(), input.size());
    // Input is required
    EXPECT_TRUE(queue.hasProcessableElement());
    // Smaller than anything
    double weightBefore = -1.0;

    // Consume everything from the queue.
    // It needs to be in increasing weight order.
    while (queue.hasProcessableElement()) {
      Step myStep = queue.pop();
      EXPECT_GE(myStep.getWeight(), weightBefore);
      weightBefore = myStep.getWeight();
    }
    // As all inputs are processable this queue shall be empty now.
    ASSERT_EQ(queue.size(), 0);
    ASSERT_FALSE(queue.hasProcessableElement());
  }
}

TEST_F(WeightedQueueTest, it_should_pop_all_loose_ends) {
  auto queue = WeightedQueue<Step>(_resourceMonitor);
  queue.append(Step{2, 1.5, true});
  queue.append(Step{3, 5, true});
  queue.append(Step{1, 1, true});
  queue.append(Step{4, 6, true});
  EXPECT_EQ(queue.size(), 4);
  EXPECT_FALSE(queue.hasProcessableElement());

  std::vector<Step*> myStepReferences = queue.getLooseEnds();
  EXPECT_EQ(myStepReferences.size(), 4);

  EXPECT_EQ(queue.size(), 4);
  EXPECT_FALSE(queue.hasProcessableElement());
}

}  // namespace queue_graph_cache_test
}  // namespace tests
}  // namespace arangodb
