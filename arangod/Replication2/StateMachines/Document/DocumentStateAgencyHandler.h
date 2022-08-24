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

#include "Basics/Result.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "RestServer/arangod.h"

#include <velocypack/Builder.h>

#include <string>
#include <memory>

namespace arangodb {
class ClusterFeature;
}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateAgencyHandler {
  virtual ~IDocumentStateAgencyHandler() = default;
  virtual auto getCollectionPlan(std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> = 0;
  virtual auto reportShardInCurrent(
      std::string const& collectionId, std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties) -> Result = 0;
};

class DocumentStateAgencyHandler : public IDocumentStateAgencyHandler {
 public:
  explicit DocumentStateAgencyHandler(GlobalLogIdentifier gid,
                                      ArangodServer& server,
                                      ClusterFeature& clusterFeature);
  auto getCollectionPlan(std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> override;
  auto reportShardInCurrent(
      std::string const& collectionId, std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> Result override;

 private:
  GlobalLogIdentifier _gid;
  ArangodServer& _server;
  ClusterFeature& _clusterFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
