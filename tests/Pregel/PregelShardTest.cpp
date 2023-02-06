////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <fmt/core.h>

#include <Inspection/VPackWithErrorT.h>
#include <Pregel/GraphStore/PregelShard.h>

#include <velocypack/Builder.h>

#include <functional>

using namespace arangodb::pregel;
using namespace arangodb::inspection;
using namespace arangodb::velocypack;

TEST(PregelShard, construction) {
  auto s = PregelShard();
  ASSERT_EQ(s, InvalidPregelShard);
  ASSERT_FALSE(s.isValid());

  auto t = PregelShard(5);
  ASSERT_NE(t, InvalidPregelShard);
  ASSERT_TRUE(t.isValid());
};

TEST(PregelShard, inspection_save) {
  const auto expected =
      fmt::format("{{\"shardID\":{}}}", PregelShard::InvalidPregelShardMarker);
  auto s = PregelShard();

  auto res = serializeWithErrorT(s);
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(res.get().toJson(), expected);
}

TEST(PregelShard, inspection_load) {
  auto input = VPackBuilder();

  input.openObject();
  input.add(VPackValue("shardID"));
  input.add(VPackValue(5));
  input.close();

  auto res = deserializeWithErrorT<PregelShard>(input.sharedSlice());
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(res.get().value, 5);
}

TEST(PregelShard, hashable) {
  auto s = PregelShard();
  auto h1 = std::hash<PregelShard>()(s);
  (void)h1;

  auto t = PregelShard(5);
  auto h2 = std::hash<PregelShard>()(t);
  (void)h2;
}
