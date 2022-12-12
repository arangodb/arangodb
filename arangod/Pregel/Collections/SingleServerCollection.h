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

#include "Collection.h"
#include "Basics/ResultT.h"
#include "Cluster/ClusterTypes.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel::collections {

struct SingleServerCollection : Collection {
 public:
  SingleServerCollection(std::shared_ptr<LogicalCollection> collection)
      : collection{collection} {}
  ~SingleServerCollection() = default;
  auto name() const -> std::string_view override;
  auto shards() const -> std::vector<ShardID> override;
  auto shardsPerServer() const -> std::unordered_map<
      ServerID, std::map<CollectionID, std::vector<ShardID>>> override;
  auto planIds() const
      -> std::unordered_map<CollectionID, std::string> override;
  auto isSystem() const -> bool override;
  auto isDeleted() const -> bool override;
  auto hasAccessRights(auth::Level requested) -> bool override;
  auto isSmart() const -> bool override;
  auto shardKeys() const -> std::vector<std::string> override;

 private:
  std::shared_ptr<LogicalCollection> collection;
};

}  // namespace arangodb::pregel::collections
