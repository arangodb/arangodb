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

#include "Replication2/StateMachines/Document/DocumentStateIndexHandler.h"
#include "Logger/LogMacros.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Indexes.h"
#include "Utils/DatabaseGuard.h"

#include <mutex>

namespace arangodb::replication2::replicated_state::document {
DocumentStateIndexHandler::DocumentStateIndexHandler(GlobalLogIdentifier gid,
                                                     TRI_vocbase_t& vocbase)
    : _gid(std::move(gid)), _vocbase(vocbase) {}

auto DocumentStateIndexHandler::ensureIndex(
    ShardID shard, std::shared_ptr<VPackBuilder> properties,
    std::shared_ptr<VPackBuilder> output,
    std::shared_ptr<methods::Indexes::ProgressTracker> progress) -> Result {
  DatabaseGuard guard(_vocbase);
  auto col = guard->lookupCollection(shard);
  if (col == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            fmt::format("Failed to lookup shard {} in database {}", shard,
                        guard->name())};
  }

  if (output == nullptr) {
    output = std::make_shared<VPackBuilder>();
  }

  return methods::Indexes::ensureIndex(*col, properties->slice(), true, *output,
                                       std::move(progress));
}
}  // namespace arangodb::replication2::replicated_state::document
