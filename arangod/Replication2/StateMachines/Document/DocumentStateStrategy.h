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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "RestServer/arangod.h"

#include <string>
#include <memory>
#include <optional>

struct TRI_vocbase_t;

namespace arangodb {
class AgencyCache;
class MaintenanceFeature;

template<typename T>
class ResultT;

namespace replication2 {
struct GlobalLogIdentifier;
class LogId;
}  // namespace replication2

namespace velocypack {
class Builder;
}
};  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {
struct IDocumentStateAgencyHandler {
  virtual ~IDocumentStateAgencyHandler() = default;
  virtual auto getCollectionPlan(std::string const& database,
                                 std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> = 0;
  virtual auto reportShardInCurrent(
      std::string const& database, std::string const& collectionId,
      std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties) -> Result = 0;
};

class DocumentStateAgencyHandler : public IDocumentStateAgencyHandler {
 public:
  explicit DocumentStateAgencyHandler(ArangodServer& server,
                                      AgencyCache& agencyCache);
  auto getCollectionPlan(std::string const& database,
                         std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> override;
  auto reportShardInCurrent(
      std::string const& database, std::string const& collectionId,
      std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> Result override;

 private:
  ArangodServer& _server;
  AgencyCache& _agencyCache;
};

struct IDocumentStateShardHandler {
  virtual ~IDocumentStateShardHandler() = default;
  virtual auto createLocalShard(
      GlobalLogIdentifier const& gid, std::string const& collectionId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> = 0;
};

class DocumentStateShardHandler : public IDocumentStateShardHandler {
 public:
  explicit DocumentStateShardHandler(MaintenanceFeature& maintenanceFeature);
  static auto stateIdToShardId(LogId logId) -> std::string;
  auto createLocalShard(GlobalLogIdentifier const& gid,
                        std::string const& collectionId,
                        std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> override;

 private:
  MaintenanceFeature& _maintenanceFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
