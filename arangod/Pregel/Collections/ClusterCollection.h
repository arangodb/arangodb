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

/**
   Collection on a cluster
In case of a smart collection, this can consist out of several collections
(which differ by their shardings). In that case, name is the name of the virtual
collection, all other functions check all underlying collections
 **/
struct ClusterCollection : Collection {
 public:
  ClusterCollection(std::string name,
                    std::vector<std::shared_ptr<LogicalCollection>> collections,
                    ClusterInfo& clusterInfo)
      : virtualName{name}, collections{collections}, clusterInfo{clusterInfo} {}
  ~ClusterCollection() = default;
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
  std::string virtualName;
  // smart edge collections contain multiple actual collections
  std::vector<std::shared_ptr<LogicalCollection>> collections;
  ClusterInfo& clusterInfo;
};

}  // namespace arangodb::pregel::collections
