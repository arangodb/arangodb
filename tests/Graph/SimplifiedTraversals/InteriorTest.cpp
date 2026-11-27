#include <gtest/gtest.h>

#include "Graph/SimplifiedTraversal/Interior.h"

namespace arangodb::graph::experimental {

TEST(InteriorTest, empty_interior_traces_empty_path) {
  Interior interior{VertexId{0}};
  EXPECT_EQ(interior.trace(Interior::Root{}), (std::vector<EdgeId>{}));
}

TEST(InteriorTest, trace_is_empty_with_invalid_start_index) {
  Interior interior{VertexId{0}};
  EXPECT_FALSE(interior.trace(size_t{1}).has_value());
}

TEST(InteriorTest, traces_back_full_paths) {
  Interior interior{VertexId{0}};
  EXPECT_EQ(interior.trace(Interior::Root{}), (std::vector<EdgeId>{}));
  auto e0Loc = interior.add(Interior::Entry{Interior::Root{}, EdgeId{0}});
  EXPECT_EQ(interior.trace(e0Loc), (std::vector<EdgeId>{{0}}));
  auto e1Loc = interior.add(Interior::Entry{e0Loc, EdgeId{1}});
  EXPECT_EQ(interior.trace(e1Loc), (std::vector<EdgeId>{{1}, {0}}));
  auto e2Loc = interior.add(Interior::Entry{e1Loc, EdgeId{2}});
  EXPECT_EQ(interior.trace(e2Loc), (std::vector<EdgeId>{{2}, {1}, {0}}));
  auto e3Loc = interior.add(Interior::Entry{e2Loc, EdgeId{3}});
  EXPECT_EQ(interior.trace(e3Loc), (std::vector<EdgeId>{{3}, {2}, {1}, {0}}));
}

TEST(InteriorTest, trace_is_empty_with_invalid_interior_entries) {
  Interior interior{VertexId{0}};
  EXPECT_EQ(interior.trace(Interior::Root{}), (std::vector<EdgeId>{}));
  // does not point to another item in interior
  auto e0Loc = interior.add(Interior::Entry{size_t{5}, EdgeId{0}});
  EXPECT_EQ(interior.trace(e0Loc), std::nullopt);
}

TEST(InteriorTest, traces_a_tree) {
  //      v0
  //   e0/ \e1
  //    x   x
  // e2/ \e3 \e4
  //  x   x   x
  Interior interior{VertexId{0}};
  auto e0Loc = interior.add(Interior::Entry{Interior::Root{}, EdgeId{0}});
  EXPECT_EQ(interior.trace(e0Loc), (std::vector<EdgeId>{{0}}));
  auto e2Loc = interior.add(Interior::Entry{e0Loc, EdgeId{2}});
  EXPECT_EQ(interior.trace(e2Loc), (std::vector<EdgeId>{{2}, {0}}));
  auto e3Loc = interior.add(Interior::Entry{e0Loc, EdgeId{3}});
  EXPECT_EQ(interior.trace(e3Loc), (std::vector<EdgeId>{{3}, {0}}));
  auto e1Loc = interior.add(Interior::Entry{Interior::Root{}, EdgeId{1}});
  EXPECT_EQ(interior.trace(e1Loc), (std::vector<EdgeId>{{1}}));
  auto e4Loc = interior.add(Interior::Entry{e1Loc, EdgeId{4}});
  EXPECT_EQ(interior.trace(e4Loc), (std::vector<EdgeId>{{4}, {1}}));
}

TEST(InteriorTest, traces_a_loop) {
  //     e0
  // v0  -->  x
  //     <--
  //      e1
  Interior interior{VertexId{0}};
  auto e0Loc = interior.add(Interior::Entry{Interior::Root{}, EdgeId{0}});
  auto e1Loc = interior.add(Interior::Entry{e0Loc, EdgeId{1}});
  auto e0Loc2 = interior.add(Interior::Entry{e1Loc, EdgeId{0}});
  EXPECT_EQ(interior.trace(e0Loc2), (std::vector<EdgeId>{{0}, {1}, {0}}));
}

}  // namespace arangodb::graph::experimental
