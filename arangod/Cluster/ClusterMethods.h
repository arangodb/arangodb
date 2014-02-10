////////////////////////////////////////////////////////////////////////////////
/// @brief methods to do things in a cluster
///
/// @file ClusterMethods.h
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

#ifndef TRIAGENS_CLUSTER_METHODS_H
#define TRIAGENS_CLUSTER_METHODS_H 1

#include "BasicsC/common.h"
#include "Basics/Common.h"

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "VocBase/voc-types.h"
#include "VocBase/update-policy.h"

#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterComm.h"

extern "C" {
  struct TRI_json_s;
}

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief merge headers of a DB server response into the current response
////////////////////////////////////////////////////////////////////////////////

    void mergeResponseHeaders (triagens::rest::HttpResponse* response,
                               std::map<std::string, std::string> const& headers);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

    std::map<std::string, std::string> getForwardableRequestHeaders (triagens::rest::HttpRequest* request);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two JSON
/// documents
////////////////////////////////////////////////////////////////////////////////

    bool shardKeysChanged (std::string const& dbname,
                           std::string const& collname,
                           struct TRI_json_s const* oldJson,
                           struct TRI_json_s const* newJson,
                           bool isPatch);

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

    int countOnCoordinator ( 
                 string const& dbname,
                 string const& collname,
                 uint64_t& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

    int createDocumentOnCoordinator ( 
                 string const& dbname,
                 string const& collname,
                 bool waitForSync,
                 TRI_json_t* json,
                 map<string, string> const& headers,
                 triagens::rest::HttpResponse::HttpResponseCode& responseCode,
                 map<string, string>& resultHeaders,
                 string& resultBody);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

    int deleteDocumentOnCoordinator ( 
                 string const& dbname,
                 string const& collname,
                 string const& key,
                 TRI_voc_rid_t const rev,
                 TRI_doc_update_policy_e policy,
                 bool waitForSync,
                 map<string, string> const& headers,
                 triagens::rest::HttpResponse::HttpResponseCode& responseCode,
                 map<string, string>& resultHeaders,
                 string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

    int getDocumentOnCoordinator ( 
                 string const& dbname,
                 string const& collname,
                 string const& key,
                 TRI_voc_rid_t const rev,
                 map<string, string> const& headers,
                 bool generateDocument,
                 triagens::rest::HttpResponse::HttpResponseCode& responseCode,
                 map<string, string>& resultHeaders,
                 string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

    int getAllDocumentsOnCoordinator ( 
                 string const& dbname,
                 string const& collname,
                 triagens::rest::HttpResponse::HttpResponseCode& responseCode,
                 string& contentType,
                 string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

    int modifyDocumentOnCoordinator ( 
                 string const& dbname,
                 string const& collname,
                 string const& key,
                 TRI_voc_rid_t const rev,
                 TRI_doc_update_policy_e policy,
                 bool waitForSync,
                 bool isPatch,
                 bool keepNull,   // only counts for isPatch == true
                 TRI_json_t* json,
                 map<string, string> const& headers,
                 triagens::rest::HttpResponse::HttpResponseCode& responseCode,
                 map<string, string>& resultHeaders,
                 string& resultBody);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an edge in a coordinator
////////////////////////////////////////////////////////////////////////////////

    int createEdgeOnCoordinator ( 
                 string const& dbname,
                 string const& collname,
                 bool waitForSync,
                 TRI_json_t* json,
                 char const* from,
                 char const* to,
                 triagens::rest::HttpResponse::HttpResponseCode& responseCode,
                 map<string, string>& resultHeaders,
                 string& resultBody);
 
// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

  }  // namespace arango
}   // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
