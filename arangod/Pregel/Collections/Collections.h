////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <map>
#include <optional>
#include "Cluster/ClusterTypes.h"
#include "Pregel/Collections/Collection.h"

namespace arangodb::pregel::collections {

struct Collections {
  std::unordered_map<CollectionID, std::shared_ptr<Collection>> collections;
  Collections(
      std::unordered_map<CollectionID, std::shared_ptr<Collection>> collections)
      : collections{std::move(collections)} {}
  auto shardsPerServer() const
      -> std::unordered_map<ServerID,
                            std::map<CollectionID, std::vector<ShardID>>>;
  auto shards() const -> std::vector<ShardID>;
  auto find(CollectionID const& id) const
      -> std::optional<std::shared_ptr<Collection>>;
  auto planIds() const -> std::unordered_map<CollectionID, std::string>;
  auto insert(Collections const& other) -> void;
};

}  // namespace arangodb::pregel::collections
