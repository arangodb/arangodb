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
#include "Containers/Concurrent/ListOfLists.h"

#include <gtest/gtest.h>
#include <memory>

using namespace arangodb::containers;

namespace {

struct MyData {
  int number;
  MyData(int number) : number{number} {}
  struct Snapshot {
    int number;
    bool operator==(Snapshot const&) const = default;
  };
  auto snapshot() const -> Snapshot { return Snapshot{.number = number}; }
};

struct MyMetrics : Metrics {
  size_t lists = 0;
  auto increment_existing_lists() -> void override { lists++; }
  auto decrement_existing_lists() -> void override { lists--; }
};

struct MyNodeList {
  using Item = MyData;
  std::vector<MyData> data;
  std::shared_ptr<Metrics> metrics;
  bool isGarbageCollected = false;

  MyNodeList(std::vector<MyData> data) : data{std::move(data)} {
    if (metrics) {
      metrics->increment_existing_lists();
    }
  }
  ~MyNodeList() {
    if (metrics) {
      metrics->decrement_existing_lists();
    }
  }
  template<typename F>
  requires std::invocable<F, MyData::Snapshot>
  auto for_node(F&& function) -> void {
    for (auto const& item : data) {
      function(item.snapshot());
    }
  }

  auto set_metrics(std::shared_ptr<Metrics> new_metrics) -> void {
    metrics = new_metrics;
  }

  auto garbage_collect_external() -> void { isGarbageCollected = true; }
};

using MyList = ListOfLists<MyNodeList>;

auto nodes_in_list(MyList& registry) -> std::vector<MyData::Snapshot> {
  std::vector<MyData::Snapshot> nodes;
  registry.for_node([&](MyData::Snapshot node) { nodes.push_back(node); });
  return nodes;
}
}  // namespace

TEST(ListOfListsTest, registers_a_list) {
  MyList list;
  auto inner_list = std::make_shared<MyNodeList>(std::vector<MyData>{1, 3, 4});

  list.add(inner_list);

  EXPECT_EQ(nodes_in_list(list),
            (std::vector<MyData::Snapshot>{{1}, {3}, {4}}));
}

TEST(ListOfListsTest, does_not_extend_lifetime_of_internal_list) {
  MyList list;

  list.add(std::make_shared<MyNodeList>(std::vector<MyData>{1, 3, 4}));

  EXPECT_EQ(nodes_in_list(list), (std::vector<MyData::Snapshot>{}));
}

TEST(ListOfListsTest, iterates_over_list_items) {
  MyList list;
  auto first_inner_list =
      std::make_shared<MyNodeList>(std::vector<MyData>{1, 2, 3});
  auto second_inner_list =
      std::make_shared<MyNodeList>(std::vector<MyData>{4, 5, 6});
  list.add(first_inner_list);
  list.add(second_inner_list);

  EXPECT_EQ(nodes_in_list(list),
            (std::vector<MyData::Snapshot>{{1}, {2}, {3}, {4}, {5}, {6}}));
}

TEST(ListOfListsTest, uses_list_of_lists_metrics_for_all_lists) {
  MyList list;
  auto inner_list = std::make_shared<MyNodeList>(std::vector<MyData>{1, 3, 4});
  list.add(inner_list);

  EXPECT_EQ(dynamic_cast<Metrics*>(list.metrics.get()),
            nullptr);  // uses default empty

  auto newMetrics = std::make_shared<MyMetrics>();
  list.set_metrics(newMetrics);
  EXPECT_NE(dynamic_cast<MyMetrics*>(list.metrics.get()), nullptr);

  auto first_inner_list =
      std::make_shared<MyNodeList>(std::vector<MyData>{1, 2, 3});
  auto second_inner_list =
      std::make_shared<MyNodeList>(std::vector<MyData>{4, 5, 6});
  list.add(first_inner_list);
  list.add(second_inner_list);

  EXPECT_NE(dynamic_cast<MyMetrics*>(list.metrics.get()), nullptr);
  EXPECT_EQ(newMetrics->lists, 2);
  EXPECT_EQ(dynamic_cast<MyMetrics*>(list.metrics.get())->lists, 2);
}

TEST(ListOfListsTest, executes_garbage_collection_on_each_list) {
  MyList list;
  auto first_inner_list =
      std::make_shared<MyNodeList>(std::vector<MyData>{1, 2, 3});
  auto second_inner_list =
      std::make_shared<MyNodeList>(std::vector<MyData>{4, 5, 6});
  list.add(first_inner_list);
  list.add(second_inner_list);

  list.run_external_cleanup();

  EXPECT_TRUE(first_inner_list->isGarbageCollected);
  EXPECT_TRUE(second_inner_list->isGarbageCollected);
}
