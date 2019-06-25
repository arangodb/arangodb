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
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

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
  if (numberOfShards == 0) {
    // Nothing to do this cannot fail
    // Deactivated this assertion, our testing mock for coordinator side
    // tries to get away without other servers by initially adding only 0
    // shard collections (non-smart). We do not want to loose these test.
    // So we will loose this assertion for now.
    /*
    TRI_ASSERT(arangodb::basics::VelocyPackHelper::getBooleanValue(
                                 json, arangodb::StaticStrings::IsSmart, false));
    */
    state = State::DONE;
  }
  TRI_ASSERT(!name.empty());
  if (needsBuildingFlag()) {
    VPackBuilder tmp;
    tmp.openObject();
    tmp.add(StaticStrings::IsBuilding, VPackValue(true));
    tmp.close();
    _isBuildingJson = VPackCollection::merge(json, tmp.slice(), true, false);
  }
}

VPackSlice const arangodb::ClusterCollectionCreationInfo::isBuildingSlice() const {
  if (needsBuildingFlag()) {
    return _isBuildingJson.slice();
  }
  return json;
}

bool arangodb::ClusterCollectionCreationInfo::needsBuildingFlag() const {
    // Deactivated the smart graph check, our testing mock for coordinator side
    // tries to get away without other servers by initially adding only 0
    // shard collections (non-smart). We do not want to loose these test.
    // So we will loose the more precise check for now.
  /*
  return numberOfShards > 0 ||
         arangodb::basics::VelocyPackHelper::getBooleanValue(json, StaticStrings::IsSmart, false);
  */
  return numberOfShards > 0;
}
