#include <cstdint>
#include <optional>
#include "gtest/gtest.h"

#include "Pregel/Messaging/Aggregate.h"

using namespace arangodb;
using namespace arangodb::pregel;

struct AddableMock {
  auto add(AddableMock const& other) -> void {}
  bool operator==(const AddableMock&) const = default;
};

TEST(PregelAggregate,
     gives_aggregated_result_only_when_components_count_is_reached) {
  auto aggregate = Aggregate<AddableMock>::withComponentsCount(2);
  ASSERT_EQ(aggregate.aggregate(AddableMock{}), std::nullopt);
  ASSERT_EQ(aggregate.aggregate(AddableMock{}), AddableMock{});
}

struct AddableStruct {
  uint64_t count;
  auto add(AddableStruct const& other) -> void { count += other.count; }
  bool operator==(const AddableStruct&) const = default;
};

TEST(PregelAggregate, aggregates_one_item) {
  auto aggregate = Aggregate<AddableStruct>::withComponentsCount(1);
  ASSERT_EQ(aggregate.aggregate(AddableStruct{.count = 3}),
            AddableStruct{.count = 3});
}

TEST(PregelAggregate, aggregates_multiple_items) {
  auto aggregate = Aggregate<AddableStruct>::withComponentsCount(2);
  ASSERT_EQ(aggregate.aggregate(AddableStruct{.count = 3}), std::nullopt);
  ASSERT_EQ(aggregate.aggregate(AddableStruct{.count = 8}),
            AddableStruct{.count = 11});
}

struct Mock {};
TEST(PregelAggregateCount, gives_true_only_when_components_count_is_reached) {
  auto aggregate = AggregateCount<Mock>(2);
  ASSERT_FALSE(aggregate.aggregate(Mock{}));
  ASSERT_TRUE(aggregate.aggregate(Mock{}));
}
