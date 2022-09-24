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

#include "DistributeShardsLike.h"

#include "Basics/Exceptions.h"

using namespace arangodb;

DistributeShardsLike::DistributeShardsLike() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result DistributeShardsLike::planShardsOnServers(
    std::vector<ServerID> availableServers,
    std::unordered_set<ServerID>& serversPlanned) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

#if false
// Precondition for DistributeShardsLike
 auto const otherCidString = basics::VelocyPackHelper::getStringValue(
   info.json, StaticStrings::DistributeShardsLike, StaticStrings::Empty);
if (!otherCidString.empty() &&
   conditions.find(otherCidString) == conditions.end()) {
 // Distribute shards like case.
 // Precondition: Master collection is not moving while we create this
 // collection We only need to add these once for every Master, we cannot
 // add multiple because we will end up with duplicate entries.
 // NOTE: We do not need to add all collections created here, as they will
 // not succeed In callbacks if they are moved during creation. If they are
 // moved after creation was reported success they are under protection by
 // Supervision.
 conditions.emplace(otherCidString);
 if (colToDistributeShardsLike != nullptr) {
   otherCidShardMap = colToDistributeShardsLike->shardIds();
 } else {
   otherCidShardMap =
       getCollection(databaseName, otherCidString)->shardIds();
 }

 auto const dslProtoColPath = paths::root()
                                  ->arango()
                                  ->plan()
                                  ->collections()
                                  ->database(databaseName)
                                  ->collection(otherCidString);
 // The distributeShardsLike prototype collection should exist in the
 // plan...
 precs.emplace_back(AgencyPrecondition(
     dslProtoColPath, AgencyPrecondition::Type::EMPTY, false));
 // ...and should not still be in creation.
 precs.emplace_back(AgencyPrecondition(dslProtoColPath->isBuilding(),
                                       AgencyPrecondition::Type::EMPTY,
                                       true));

 // Any of the shards locked?
 for (auto const& shard : *otherCidShardMap) {
   precs.emplace_back(
       AgencyPrecondition("Supervision/Shards/" + shard.first,
                          AgencyPrecondition::Type::EMPTY, true));
 }

#endif
