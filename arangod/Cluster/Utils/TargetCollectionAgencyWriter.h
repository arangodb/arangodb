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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Guarded.h"
#include "Containers/FlatHashMap.h"
#include "Cluster/Utils/CollectionGroupUpdates.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb {

namespace velocypack {
template<typename T>
class Buffer;

class Slice;
}  // namespace velocypack

class Result;

template<typename T>
class ResultT;

struct AgencyWriteTransaction;
struct CurrentWatcher;
struct IShardDistributionFactory;
struct PlanCollectionEntryReplication2;
class AgencyCache;

struct TargetCollectionAgencyWriter {
  TargetCollectionAgencyWriter(
      std::vector<PlanCollectionEntryReplication2> collectionPlanEntries,
      std::unordered_map<std::string,
                         std::shared_ptr<IShardDistributionFactory>>
          shardDistributionsUsed,
      replication2::CollectionGroupUpdates collectionGroups);

  [[nodiscard]] std::shared_ptr<CurrentWatcher> prepareCurrentWatcher(
      std::string_view databaseName, bool waitForSyncReplication,
      AgencyCache& agencyCache) const;

  [[nodiscard]] ResultT<velocypack::Buffer<uint8_t>> prepareCreateTransaction(
      std::string_view databaseName) const;

  [[nodiscard]] std::vector<std::string> collectionNames() const;

 private:
  // Information required for the collections to write

  std::vector<PlanCollectionEntryReplication2> _collectionPlanEntries;
  std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>
      _shardDistributionsUsed;
  replication2::CollectionGroupUpdates _collectionGroups;
};

}  // namespace arangodb
