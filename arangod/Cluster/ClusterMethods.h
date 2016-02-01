////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_METHODS_H
#define ARANGOD_CLUSTER_CLUSTER_METHODS_H 1

#include "Basics/Common.h"

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-types.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterComm.h"

struct TRI_json_t;
struct TRI_vector_pointer_s;

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

namespace traverser {
class TraverserExpression;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge headers of a DB server response into the current response
////////////////////////////////////////////////////////////////////////////////

void mergeResponseHeaders(arangodb::rest::HttpResponse* response,
                          std::map<std::string, std::string> const& headers);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> getForwardableRequestHeaders(
    arangodb::rest::HttpRequest* request);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two JSON
/// documents
////////////////////////////////////////////////////////////////////////////////

bool shardKeysChanged(std::string const& dbname, std::string const& collname,
                      struct TRI_json_t const* oldJson,
                      struct TRI_json_t const* newJson, bool isPatch);

bool shardKeysChanged(std::string const& dbname, std::string const& collname,
                      VPackSlice const& oldSlice, VPackSlice const& newSlice,
                      bool isPatch);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns users
////////////////////////////////////////////////////////////////////////////////

int usersOnCoordinator(std::string const& dbname,
                       arangodb::velocypack::Builder& result, double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int revisionOnCoordinator(std::string const& dbname,
                          std::string const& collname, TRI_voc_rid_t&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int figuresOnCoordinator(std::string const& dbname, std::string const& collname,
                         TRI_doc_collection_info_t*&);

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

int countOnCoordinator(std::string const& dbname, std::string const& collname,
                       uint64_t& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname, bool waitForSync,
    arangodb::velocypack::Slice const& slice,
    std::map<std::string, std::string> const& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname, bool waitForSync,
    std::unique_ptr<TRI_json_t>& json,
    std::map<std::string, std::string> const& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int deleteDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    TRI_doc_update_policy_e policy, bool waitForSync,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    bool generateDocument,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a list of filtered documents in a coordinator
///        All found documents will be inserted into result.
///        After execution documentIds will contain all id's of documents
///        that could not be found.
////////////////////////////////////////////////////////////////////////////////

int getFilteredDocumentsOnCoordinator(
    std::string const& dbname,
    std::vector<traverser::TraverserExpression*> const& expressions,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    std::unordered_set<std::string>& documentIds,
    std::unordered_map<std::string, TRI_json_t*>& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getAllDocumentsOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& returnType,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::string& contentType, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get all edges in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getAllEdgesOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& vertex, TRI_edge_direction_e const& direction,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::string& contentType, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a filtered set of edges on Coordinator.
///        Also returns the result in Json
////////////////////////////////////////////////////////////////////////////////

int getFilteredEdgesOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& vertex, TRI_edge_direction_e const& direction,
    std::vector<traverser::TraverserExpression*> const& expressions,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::string& contentType, arangodb::basics::Json& resultJson);

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    TRI_doc_update_policy_e policy, bool waitForSync, bool isPatch,
    bool keepNull,      // only counts for isPatch == true
    bool mergeObjects,  // only counts for isPatch == true
    arangodb::velocypack::Slice const& slice,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    TRI_doc_update_policy_e policy, bool waitForSync, bool isPatch,
    bool keepNull,      // only counts for isPatch == true
    bool mergeObjects,  // only counts for isPatch == true
    std::unique_ptr<TRI_json_t>& json,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an edge in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createEdgeOnCoordinator(
    std::string const& dbname, std::string const& collname, bool waitForSync,
    std::unique_ptr<TRI_json_t>& json, char const* from, char const* to,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders, std::string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

int truncateCollectionOnCoordinator(std::string const& dbname,
                                    std::string const& collname);

////////////////////////////////////////////////////////////////////////////////
/// @brief flush Wal on all DBservers
////////////////////////////////////////////////////////////////////////////////

int flushWalOnAllDBServers(bool, bool);

}  // namespace arangodb

#endif
