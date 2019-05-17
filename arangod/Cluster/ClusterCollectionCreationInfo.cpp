////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019-2019 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterCollectionCreationInfo.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

arangodb::ClusterCollectionCreationInfo::ClusterCollectionCreationInfo(
    std::string const cID, uint64_t shards, uint64_t repFac, bool waitForRep,
    velocypack::Slice const& slice)
    : collectionID(std::move(cID)),
      numberOfShards(shards),
      replicationFactor(repFac),
      waitForReplication(waitForRep),
      json(slice),
      name(arangodb::basics::VelocyPackHelper::getStringValue(json, arangodb::StaticStrings::DataSourceName,
                                                              StaticStrings::Empty)),
      state(State::INIT) {
  if (numberOfShards == 0 || arangodb::basics::VelocyPackHelper::getBooleanValue(
                                 json, arangodb::StaticStrings::IsSmart, false)) {
    // Nothing to do this cannot fail
    state = State::DONE;
  }
  TRI_ASSERT(!name.empty());
  VPackBuilder tmp;
  tmp.openObject();
  tmp.add(StaticStrings::IsBuilding, VPackValue(true));
  tmp.close();
  isBuildingJson = VPackCollection::merge(json, tmp.slice(), true, false);
}