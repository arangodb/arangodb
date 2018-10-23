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

#include "ClusterRestCollectionHandler.h"
#include "ClusterEngine/MMFilesMethods.h"
#include "ClusterEngine/RocksDBMethods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

ClusterRestCollectionHandler::ClusterRestCollectionHandler(GeneralRequest* request,
                                             GeneralResponse* response)
    : RestCollectionHandler(request, response) {}

Result ClusterRestCollectionHandler::handleExtraCommandPut(LogicalCollection& coll,
                                                           std::string const& suffix,
                                                           velocypack::Builder& builder) {
  
  if (suffix == "recalculateCount") {
    Result res = arangodb::rocksdb::recalculateCountsOnAllDBServers(_vocbase.name(), coll.name());
    if (res.ok()) {
      VPackObjectBuilder guard(&builder);
      builder.add("result", VPackValue(true));
    }
    return res;
  } else if (suffix == "rotate") {
    Result res = arangodb::mmfiles::rotateActiveJournalOnAllDBServers(_vocbase.name(), coll.name());
    if (res.ok()) {
      VPackObjectBuilder guard(&builder);
      builder.add("result", VPackValue(true));
    }
    return res;
  }
  
  return TRI_ERROR_NOT_IMPLEMENTED;
}
