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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <string_view>

namespace arangodb {

class ClusterFeature;
class Result;

namespace velocypack {
class Builder;
}

/// @brief flush Wal on all DBservers
Result flushWalOnAllDBServers(ClusterFeature&, bool waitForSync,
                              bool flushColumnFamilies);

/// @brief recalculate collection count on all DBServers
Result recalculateCountsOnAllDBServers(ClusterFeature&, std::string_view dbname,
                                       std::string_view collname);

/// @brief compact the database on all DB servers
Result compactOnAllDBServers(ClusterFeature&, bool changeLevel,
                             bool compactBottomMostLevel);

/// @brief compact the data of a single collection on all DB servers
Result compactOnAllDBServers(ClusterFeature&, std::string const& dbname,
                             std::string const& collname);

/// @brief get the engine stats from all DB servers
Result getEngineStatsFromDBServers(ClusterFeature&,
                                   velocypack::Builder& report);

}  // namespace arangodb
