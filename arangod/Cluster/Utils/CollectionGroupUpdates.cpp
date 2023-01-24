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
    UserInputCollectionProperties const& collection) {
  auto newId = agency::CollectionGroupId(collection.id.id());
  agency::CollectionGroup g;
  g.id = newId;
  g.attributes.waitForSync = false;
  g.attributes.writeConcern = 1;
  g.collections.emplace(::toCollectionIdString(collection.id),
                        agency::CollectionGroup::Collection{});
  newGroups.emplace_back(std::move(g));
  return newId;
}

void CollectionGroupUpdates::addToNewGroup(
    agency::CollectionGroupId const& groupId, arangodb::DataSourceId cid) {
  // Performance: We could make this a map id -> group, however in most cases
  // this vector will have 1 entry only
  if (auto entry = std::ranges::find_if(
          newGroups, [&groupId](auto const& g) { return g.id == groupId; });
      entry != newGroups.end()) {
    entry->collections.emplace(::toCollectionIdString(cid),
                               agency::CollectionGroup::Collection{});
  } else {
    TRI_ASSERT(false) << "Tried to add to groupId" << groupId
                      << " which should be added in the same creation.";
  }
}

void CollectionGroupUpdates::addToExistingGroup(
    agency::CollectionGroupId const& groupId, arangodb::DataSourceId cid) {
  additionsToGroup.emplace_back(
      AddCollectionToGroup{groupId, ::toCollectionIdString(cid)});
}
