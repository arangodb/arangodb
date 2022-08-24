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
      std::string const& collectionId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> = 0;
};

class DocumentStateShardHandler : public IDocumentStateShardHandler {
 public:
  explicit DocumentStateShardHandler(GlobalLogIdentifier gid,
                                     MaintenanceFeature& maintenanceFeature);
  static auto stateIdToShardId(LogId logId) -> std::string;
  auto createLocalShard(std::string const& collectionId,
                        std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> override;

 private:
  GlobalLogIdentifier _gid;
  MaintenanceFeature& _maintenanceFeature;
};

}  // namespace arangodb::replication2::replicated_state::document
