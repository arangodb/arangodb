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

#include "CollectionGroupUpdates.h"

#include "Basics/debugging.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"

using namespace arangodb;
using namespace arangodb::replication2;

namespace {
CollectionID toCollectionIdString(arangodb::DataSourceId const& cid) {
  return std::to_string(cid.id());
}
}  // namespace

agency::CollectionGroupId CollectionGroupUpdates::addNewGroup(
    UserInputCollectionProperties const& collection,
    std::function<uint64_t()> const& generateId) {
  auto newId = agency::CollectionGroupId(generateId());
  agency::CollectionGroupTargetSpecification g;
  g.id = newId;
  g.version = 1;
  g.attributes.mutableAttributes.waitForSync = collection.waitForSync;
  g.attributes.mutableAttributes.writeConcern = collection.writeConcern.value();
  g.attributes.mutableAttributes.replicationFactor =
      collection.replicationFactor.value();
  g.attributes.immutableAttributes.numberOfShards =
      collection.numberOfShards.value();
  g.collections.emplace(::toCollectionIdString(collection.id),
                        agency::CollectionGroup::Collection{});
  newGroups.emplace_back(std::move(g));
  return newId;
}

void CollectionGroupUpdates::addToNewGroup(
    agency::CollectionGroupId const& groupId, arangodb::DataSourceId cid) {
  // Performance: We could make this a map id -> group, however in most cases
  // this vector will have 1 entry only (it will be used in Graphs and
  // CreateDatabase)
  for (auto& entry : newGroups) {
    if (entry.id == groupId) {
      entry.collections.emplace(::toCollectionIdString(cid),
                                agency::CollectionGroup::Collection{});
      break;
    }
  }
}

void CollectionGroupUpdates::addToExistingGroup(
    agency::CollectionGroupId const& groupId, arangodb::DataSourceId cid) {
  additionsToGroup.emplace_back(
      AddCollectionToGroup{groupId, ::toCollectionIdString(cid)});
}

auto CollectionGroupUpdates::getAllModifiedGroups() const noexcept
    -> std::unordered_set<agency::CollectionGroupId> {
  std::unordered_set<agency::CollectionGroupId> modifiedGroups;
  modifiedGroups.reserve(additionsToGroup.size());
  for (auto const& add : additionsToGroup) {
    modifiedGroups.emplace(add.id);
  }
  return modifiedGroups;
}
