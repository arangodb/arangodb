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

#include "Cluster/ClusterTypes.h"
#include "Replication2/AgencyCollectionSpecification.h"

#include <unordered_set>

namespace arangodb {
struct UserInputCollectionProperties;
class DataSourceId;
}  // namespace arangodb

namespace arangodb::replication2 {

struct AddCollectionToGroup {
  agency::CollectionGroupId id;
  CollectionID collectionId;
};

struct CollectionGroupUpdates {
  std::vector<agency::CollectionGroupTargetSpecification> newGroups;
  std::vector<AddCollectionToGroup> additionsToGroup;

  agency::CollectionGroupId addNewGroup(
      UserInputCollectionProperties const& collection,
      std::function<uint64_t()> const& generateId);

  void addToNewGroup(agency::CollectionGroupId const& groupId,
                     arangodb::DataSourceId cid);
  void addToExistingGroup(agency::CollectionGroupId const& groupId,
                          arangodb::DataSourceId cid);

  [[nodiscard]] auto getAllModifiedGroups() const noexcept
      -> std::unordered_set<agency::CollectionGroupId>;
};

}  // namespace arangodb::replication2
