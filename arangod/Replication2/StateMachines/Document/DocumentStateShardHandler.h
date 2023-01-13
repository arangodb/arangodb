////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/ResultT.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

#include <velocypack/Builder.h>

#include <string>
#include <memory>

namespace arangodb {
class MaintenanceFeature;
}

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateShardHandler {
  virtual ~IDocumentStateShardHandler() = default;
  virtual auto createLocalShard(
      ShardID const& shardId, std::string const& collectionId,
      std::shared_ptr<velocypack::Builder> const& properties) -> Result = 0;
  virtual auto dropLocalShard(ShardID const& shardId,
                              std::string const& collectionId) -> Result = 0;
};

class DocumentStateShardHandler : public IDocumentStateShardHandler {
 public:
  explicit DocumentStateShardHandler(GlobalLogIdentifier gid,
                                     MaintenanceFeature& maintenanceFeature);
  auto createLocalShard(ShardID const& shardId, std::string const& collectionId,
                        std::shared_ptr<velocypack::Builder> const& properties)
      -> Result override;
  auto dropLocalShard(ShardID const& shardId, const std::string& collectionId)
      -> Result override;

 private:
  GlobalLogIdentifier _gid;
  MaintenanceFeature& _maintenanceFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
