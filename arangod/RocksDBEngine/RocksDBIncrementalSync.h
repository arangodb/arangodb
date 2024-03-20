////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
class LogicalCollection;

Result syncChunkRocksDB(DatabaseInitialSyncer& syncer,
                        SingleCollectionTransaction* trx,
                        ReplicationMetricsFeature::InitialSyncStats& stats,
                        std::string const& keysId, uint64_t chunkId,
                        std::string const& lowString,
                        std::string const& highString,
                        std::vector<std::string> const& markers);

Result handleSyncKeysRocksDB(DatabaseInitialSyncer& syncer,
                             arangodb::LogicalCollection* col,
                             std::string const& keysId);
}  // namespace arangodb
