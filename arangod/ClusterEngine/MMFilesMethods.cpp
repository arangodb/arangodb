////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesMethods.h"

#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace mmfiles {

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the active journals for the collection on all DBServers
////////////////////////////////////////////////////////////////////////////////

int rotateActiveJournalOnAllDBServers(std::string const& dbname, std::string const& collname) {
  ClusterEngine* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
  if (!ce->isMMFiles()) {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }

  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }

  // First determine the collection ID from the name:
  auto collinfo = ci->getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  std::string const baseUrl =
      "/_db/" + basics::StringUtils::urlEncode(dbname) + "/_api/collection/";
  std::shared_ptr<std::string> body;

  // now we notify all leader and follower shards
  std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
  std::vector<ClusterCommRequest> requests;
  for (auto const& shard : *shardList) {
    for (ServerID const& server : shard.second) {
      std::string uri =
          baseUrl + basics::StringUtils::urlEncode(shard.first) + "/rotate";
      requests.emplace_back("server:" + server, arangodb::rest::RequestType::PUT,
                            std::move(uri), body);
    }
  }

  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 600.0, nrDone, Logger::ENGINES, false);

  if (nrGood < requests.size()) {
    return TRI_ERROR_FAILED;
  }
  return TRI_ERROR_NO_ERROR;
}

}  // namespace mmfiles
}  // namespace arangodb
