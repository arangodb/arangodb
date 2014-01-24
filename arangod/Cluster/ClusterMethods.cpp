////////////////////////////////////////////////////////////////////////////////
/// @brief methods to do things in a cluster
///
/// @file ClusterMethods.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterMethods.h"

#include "BasicsC/json.h"
#include "Basics/StringUtils.h"
#include "VocBase/server.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator (
                string const& dbname,
                string const& collname,
                bool waitForSync,
                TRI_json_t* json,
                triagens::rest::HttpResponse::HttpResponseCode& responseCode,
                string& contentType,
                string& resultBody) {

  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  CollectionInfo collinfo = ci->getCollection(dbname, collname);
  if (collinfo.empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  string collid = StringUtils::itoa(collinfo.id());

  // Sort out the _key attribute:
  // The user is allowed to specify _key, provided that _key is the one
  // and only sharding attribute, because in this case we can delegate
  // the responsibility to make _key attributes unique to the responsible
  // shard. Otherwise, we ensure uniqueness here and now by taking a
  // cluster-wide unique number. Note that we only know the sharding 
  // attributes a bit further down the line when we have determined
  // the responsible shard.
  TRI_json_t* subjson = TRI_LookupArrayJson(json, "_key");
  bool userSpecifiedKey = false;
  string _key;
  if (0 == subjson) {
    // The user did not specify a key, let's create one:
    uint64_t uid = ci->uniqid();
    _key = triagens::basics::StringUtils::itoa(uid);
    TRI_InsertArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "_key",
                        TRI_CreateStringReference2Json(TRI_UNKNOWN_MEM_ZONE, 
                                                     _key.c_str(), _key.size()));
  }
  else {
    userSpecifiedKey = true;
  }

  // Now find the responsible shard:
  bool usesDefaultShardingAttributes;
  ShardID shardID = ci->getResponsibleShard( collid, json, true,
                                             usesDefaultShardingAttributes );
  if (shardID == "") {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return TRI_ERROR_SHARD_GONE;
  }

  // Now perform the above mentioned check:
  if (userSpecifiedKey && !usesDefaultShardingAttributes) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
  }

  string body = JsonHelper::toString(json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  // Send a synchronous request to that shard using ClusterComm:
  ClusterCommResult* res;
  map<string, string> headers;
  res = cc->syncRequest("", TRI_NewTickServer(), "shard:"+shardID,
                        triagens::rest::HttpRequest::HTTP_REQUEST_POST,
                        "/_db/"+dbname+"/_api/document?collection="+
                        StringUtils::urlEncode(shardID)+"&waitForSync="+
                        (waitForSync ? "true" : "false"),
                        body.c_str(), body.size(), headers, 60.0);
  
  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    delete res;
    return TRI_ERROR_CLUSTER_TIMEOUT;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    if (!res->result->isComplete()) {
      delete res;
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
    // In this case a proper HTTP error was reported by the DBserver,
    // this can be 400 or 404, we simply forward the result.
    // We intentionally fall through here.
  }
  responseCode = static_cast<triagens::rest::HttpResponse::HttpResponseCode>
                            (res->result->getHttpReturnCode());
  contentType = res->result->getContentType(false);
  resultBody = res->result->getBody().str();
  delete res;
  return TRI_ERROR_NO_ERROR;
}

  }  // namespace arango
}  // namespace triagens

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


