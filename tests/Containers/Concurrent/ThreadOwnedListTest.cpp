////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#include "Containers/Concurrent/ThreadOwnedList.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <source_location>
#include <thread>

using namespace arangodb::containers;

namespace {

struct InstanceCounterValue {
  InstanceCounterValue() { instanceCounter += 1; }
  InstanceCounterValue(InstanceCounterValue const& o) { instanceCounter += 1; }
  InstanceCounterValue(InstanceCounterValue&& o) noexcept {
    instanceCounter += 1;
  }

  ~InstanceCounterValue() {
    if (instanceCounter == 0) {
      abort();
    }
    instanceCounter -= 1;
  }
  static std::size_t instanceCounter;
};
std::size_t InstanceCounterValue::instanceCounter = 0;

struct NodeData : InstanceCounterValue {
  int number;
  bool isDeleted = false;

  NodeData(int number) : number{number} {}

  struct Snapshot {
    int number;
    bool operator==(Snapshot const&) const = default;
  };
  auto snapshot() -> Snapshot { return Snapshot{.number = number}; }
  auto set_to_deleted() -> void { isDeleted = true; };
};

using MyList = ThreadOwnedList<NodeData>;

auto nodes_in_registry(std::shared_ptr<MyList> registry)
    -> std::vector<NodeData::Snapshot> {
  std::vector<NodeData::Snapshot> nodes;
  registry->for_node(
      [&](NodeData::Snapshot promise) { nodes.push_back(promise); });
  return nodes;
}
}  // namespace

struct ThreadOwnedListTest : ::testing::Test {
  void TearDown() override {
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
  }
};
using ThreadOwnedListDeathTest = ThreadOwnedListTest;

TEST_F(ThreadOwnedListTest, adds_a_promise) {
  auto registry = MyList::make();

  auto node = registry->add([]() { return NodeData{2}; });

  EXPECT_EQ(nodes_in_registry(registry),
            (std::vector<NodeData::Snapshot>{node->data.snapshot()}));

  // make sure registry is cleaned up
  registry->mark_for_deletion(node);
}

TEST_F(ThreadOwnedListDeathTest, another_thread_cannot_add_a_promise) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  auto registry = MyList::make();

  std::jthread([&]() {
    EXPECT_DEATH(registry->add([]() { return NodeData{1}; }),
                 "Assertion failed");
  });
}

TEST_F(ThreadOwnedListTest, iterates_over_all_promises) {
  auto registry = MyList::make();

  auto* first_node = registry->add([]() { return NodeData{5}; });
  auto* second_node = registry->add([]() { return NodeData{9}; });
  auto* third_node = registry->add([]() { return NodeData{10}; });

  EXPECT_EQ(nodes_in_registry(registry),
            (std::vector<NodeData::Snapshot>{third_node->data.snapshot(),
                                             second_node->data.snapshot(),
                                             first_node->data.snapshot()}));

  // make sure registry is cleaned up
  registry->mark_for_deletion(first_node);
  registry->mark_for_deletion(second_node);
  registry->mark_for_deletion(third_node);
}

TEST_F(ThreadOwnedListTest, iterates_in_another_thread_over_all_promises) {
  auto registry = MyList::make();

  auto* first_node = registry->add([]() { return NodeData{19}; });
  auto* second_node = registry->add([]() { return NodeData{0}; });
  auto* third_node = registry->add([]() { return NodeData{3}; });

  std::thread([&]() {
    EXPECT_EQ(nodes_in_registry(registry),
              (std::vector<NodeData::Snapshot>{third_node->data.snapshot(),
                                               second_node->data.snapshot(),
                                               first_node->data.snapshot()}));
  }).join();

  // make sure registry is cleaned up
  registry->mark_for_deletion(first_node);
  registry->mark_for_deletion(second_node);
  registry->mark_for_deletion(third_node);
}

TEST_F(ThreadOwnedListTest, marked_promises_are_deleted_in_garbage_collection) {
  auto registry = MyList::make();
  auto* node_to_delete = registry->add([]() { return NodeData{1}; });
  auto* another_node = registry->add([]() { return NodeData{77}; });

  registry->mark_for_deletion(node_to_delete);
  EXPECT_EQ(nodes_in_registry(registry),
            (std::vector<NodeData::Snapshot>{another_node->data.snapshot(),
                                             node_to_delete->data.snapshot()}));
  EXPECT_TRUE(node_to_delete->data.isDeleted);
  EXPECT_FALSE(another_node->data.isDeleted);

  registry->garbage_collect();
  EXPECT_EQ(nodes_in_registry(registry),
            (std::vector<NodeData::Snapshot>{another_node->data.snapshot()}));

  // make sure registry is cleaned up
  registry->mark_for_deletion(another_node);
}

TEST_F(ThreadOwnedListTest, garbage_collection_deletes_marked_promises) {
  {
    auto registry = MyList::make();
    auto* first_node = registry->add([]() { return NodeData{21}; });
    auto* second_node = registry->add([]() { return NodeData{1}; });
    auto* third_node = registry->add([]() { return NodeData{100}; });

    registry->mark_for_deletion(first_node);
    registry->garbage_collect();

    EXPECT_EQ(nodes_in_registry(registry),
              (std::vector<NodeData::Snapshot>{third_node->data.snapshot(),
                                               second_node->data.snapshot()}));

    // clean up
    registry->mark_for_deletion(second_node);
    registry->mark_for_deletion(third_node);
  }
  {
    auto registry = MyList::make();
    auto* first_node = registry->add([]() { return NodeData{21}; });
    auto* second_node = registry->add([]() { return NodeData{1}; });
    auto* third_node = registry->add([]() { return NodeData{100}; });

    registry->mark_for_deletion(second_node);
    registry->garbage_collect();

    EXPECT_EQ(nodes_in_registry(registry),
              (std::vector<NodeData::Snapshot>{third_node->data.snapshot(),
                                               first_node->data.snapshot()}));

    // clean up
    registry->mark_for_deletion(first_node);
    registry->mark_for_deletion(third_node);
  }
  {
    auto registry = MyList::make();
    auto* first_node = registry->add([]() { return NodeData{21}; });
    auto* second_node = registry->add([]() { return NodeData{1}; });
    auto* third_node = registry->add([]() { return NodeData{100}; });

    registry->mark_for_deletion(third_node);
    registry->garbage_collect();

    EXPECT_EQ(nodes_in_registry(registry),
              (std::vector<NodeData::Snapshot>{second_node->data.snapshot(),
                                               first_node->data.snapshot()}));

    // clean up
    registry->mark_for_deletion(first_node);
    registry->mark_for_deletion(second_node);
  }
}

TEST_F(ThreadOwnedListDeathTest,
       unrelated_promise_cannot_be_marked_for_deletion) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  auto registry = MyList::make();
  auto* promise = registry->add([]() { return NodeData{33}; });

  auto some_other_registry = MyList::make();
  EXPECT_DEATH(some_other_registry->mark_for_deletion(promise),
               "Assertion failed");

  // correct clean up
  registry->mark_for_deletion(promise);
}

TEST_F(ThreadOwnedListTest, another_thread_can_mark_a_promise_for_deletion) {
  auto registry = MyList::make();

  auto* node_to_delete = registry->add([]() { return NodeData{7}; });
  auto* another_node = registry->add([]() { return NodeData{4}; });

  std::thread([&]() { registry->mark_for_deletion(node_to_delete); }).join();

  registry->garbage_collect();
  EXPECT_EQ(nodes_in_registry(registry),
            (std::vector<NodeData::Snapshot>{another_node->data.snapshot()}));

  // clean up
  registry->mark_for_deletion(another_node);
}

TEST_F(ThreadOwnedListDeathTest,
       garbage_collection_cannot_be_called_on_different_thread) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  auto registry = MyList::make();

  std::jthread(
      [&] { EXPECT_DEATH(registry->garbage_collect(), "Assertion failed"); });
}
