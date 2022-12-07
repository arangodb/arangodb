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

#include <memory>
#include <map>
#include "Auth/Common.h"
#include "Cluster/ClusterTypes.h"

namespace arangodb::pregel::collections {

/**
   Interface for a collection in the database
 **/
struct Collection {
  virtual ~Collection() = default;
  virtual auto name() const -> std::string_view = 0;
  virtual auto shards() const -> std::vector<ShardID> = 0;
  virtual auto shardsPerServer() const
      -> std::unordered_map<ServerID,
                            std::map<CollectionID, std::vector<ShardID>>> = 0;
  virtual auto planIds() const
      -> std::unordered_map<CollectionID, std::string> = 0;
  virtual auto isSystem() const -> bool = 0;
  virtual auto isDeleted() const -> bool = 0;
  virtual auto hasAccessRights(auth::Level requested) -> bool = 0;
  virtual auto isSmart() const -> bool = 0;
  virtual auto shardKeys() const -> std::vector<std::string> = 0;
};

}  // namespace arangodb::pregel::collections
