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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb {

namespace velocypack {
class Slice;
}

class Result;

template<typename T>
class ResultT;

struct AgencyWriteTransaction;
struct CurrentWatcher;
struct PlanCollectionEntry;
struct IShardDistributionFactory;

struct PlanCollectionToAgencyWriter {
  PlanCollectionToAgencyWriter(
      std::vector<arangodb::PlanCollectionEntry> collectionPlanEntries,
      std::unordered_map<std::string,
                         std::shared_ptr<IShardDistributionFactory>>
          shardDistributionsUsed);

  [[nodiscard]] std::shared_ptr<CurrentWatcher> prepareCurrentWatcher(
      std::string_view databaseName, bool waitForSyncReplication) const;

  [[nodiscard]] AgencyWriteTransaction prepareUndoTransaction(
      std::string_view databaseName) const;

  [[nodiscard]] ResultT<AgencyWriteTransaction> prepareStartBuildingTransaction(
      std::string_view databaseName, uint64_t planVersion,
      std::vector<std::string> serversAvailable) const;

  [[nodiscard]] AgencyWriteTransaction prepareCompletedTransaction(
      std::string_view databaseName);

  [[nodiscard]] std::vector<std::string> collectionNames() const;

 private:
  // Information required for the collections to write

  std::vector<arangodb::PlanCollectionEntry> _collectionPlanEntries;
  std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>
      _shardDistributionsUsed;
};

}  // namespace arangodb
