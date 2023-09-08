////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Result.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "VocBase/Methods/Indexes.h"

#include <set>
#include <shared_mutex>

struct TRI_vocbase_t;

namespace arangodb {
class MaintenanceFeature;

namespace replication2::replicated_state::document {
struct IDocumentStateIndexHandler {
  virtual ~IDocumentStateIndexHandler() = default;
  virtual auto ensureIndex(
      ShardID shard, std::shared_ptr<VPackBuilder> properties,
      std::shared_ptr<VPackBuilder> output,
      std::shared_ptr<methods::Indexes::ProgressTracker> progress)
      -> Result = 0;
};

class DocumentStateIndexHandler : public IDocumentStateIndexHandler {
  using IndexID = std::pair<ShardID, std::string>;

 public:
  DocumentStateIndexHandler(GlobalLogIdentifier gid, TRI_vocbase_t& vocbase);
  auto ensureIndex(ShardID shard, std::shared_ptr<VPackBuilder> properties,
                   std::shared_ptr<VPackBuilder> output,
                   std::shared_ptr<methods::Indexes::ProgressTracker> progress)
      -> Result override;

 private:
  GlobalLogIdentifier _gid;
  TRI_vocbase_t& _vocbase;
  struct {
    std::set<IndexID> indexes;
    std::shared_mutex mutex;
  } _indexes;
};

}  // namespace replication2::replicated_state::document
}  // namespace arangodb
