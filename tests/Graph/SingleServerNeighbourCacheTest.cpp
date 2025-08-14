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
#include "Graph/Providers/SingleServer/NeighbourCache.h"

#include <gtest/gtest.h>
#include <velocypack/HashedStringRef.h>

using namespace arangodb;
using namespace arangodb::graph;

struct MyMonitor {
  auto increaseMemoryUsage(std::uint64_t value) -> void {}
  auto decreaseMemoryUsage(std::uint64_t value) -> void {}
};

TEST(SingleServerNeighbourCacheTest,
     gives_vertex_batches_that_were_added_to_cache) {
  auto monitor = MyMonitor{};
  auto cache = NeighbourCache{};
  std::string vertexName = "abc";
  auto vertex = velocypack::HashedStringRef{vertexName.c_str(), 3};

  // rearm cache to new vertex
  ASSERT_EQ(cache.rearm(vertex), std::nullopt);

  // add a batch to vertex in cache
  auto vec = std::vector<ExpansionInfo>{};
  vec.emplace_back(ExpansionInfo{
      EdgeDocumentToken{DataSourceId{4}, LocalDocumentId{8}}, VPackSlice{}, 0});
  auto first_batch =
      std::make_shared<std::vector<ExpansionInfo>>(std::move(vec));
  cache.update(first_batch, monitor);
  ASSERT_EQ(cache.rearm(vertex), std::nullopt);

  // add another batch to vertex in cache and make this the last batch for this
  // vertex
  vec = std::vector<ExpansionInfo>{};
  vec.emplace_back(ExpansionInfo{
      EdgeDocumentToken{DataSourceId{5}, LocalDocumentId{9}}, VPackSlice{}, 0});
  auto second_batch =
      std::make_shared<std::vector<ExpansionInfo>>(std::move(vec));
  cache.update(second_batch, monitor, true);

  auto iterator = cache.rearm(vertex);
  ASSERT_TRUE(iterator != std::nullopt);

  // now get items
  ASSERT_EQ(*iterator->next(), first_batch);
  ASSERT_EQ(*iterator->next(), second_batch);
  ASSERT_EQ(iterator->next(), std::nullopt);

  // add_batch for another vertex
  std::string anotherVertexName = "def";
  auto another_vertex =
      velocypack::HashedStringRef{anotherVertexName.c_str(), 87};
  ASSERT_EQ(cache.rearm(another_vertex), std::nullopt);
  vec = std::vector<ExpansionInfo>{};
  vec.emplace_back(ExpansionInfo{
      EdgeDocumentToken{DataSourceId{4}, LocalDocumentId{8}}, VPackSlice{}, 0});
  auto first_batch_for_new_vertex =
      std::make_shared<std::vector<ExpansionInfo>>(std::move(vec));
  cache.update(first_batch_for_new_vertex, monitor, true);

  auto another_iterator = cache.rearm(another_vertex);
  ASSERT_TRUE(another_iterator != std::nullopt);

  // now get items of another vertex
  ASSERT_EQ(*another_iterator->next(), first_batch_for_new_vertex);
  ASSERT_EQ(another_iterator->next(), std::nullopt);
}
