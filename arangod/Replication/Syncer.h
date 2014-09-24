////////////////////////////////////////////////////////////////////////////////
/// @brief replication syncer base class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REPLICATION_SYNCER_H
#define ARANGODB_REPLICATION_SYNCER_H 1

#include "Basics/Common.h"
#include "Basics/logging.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-master.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_json_t;
struct TRI_replication_applier_configuration_s;
struct TRI_transaction_collection_s;
struct TRI_vocbase_s;
struct TRI_vocbase_col_s;

namespace triagens {

  namespace httpclient {
    class GeneralClientConnection;
    class SimpleHttpClient;
    class SimpleHttpResult;
  }

  namespace rest {
    class Endpoint;
  }

  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                            Syncer
// -----------------------------------------------------------------------------

    class Syncer {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        Syncer (struct TRI_vocbase_s*,
                struct TRI_replication_applier_configuration_s const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~Syncer ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

        static string rewriteLocation (void*, const string&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from JSON
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t getCid (struct TRI_json_t const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a single marker from the collection dump
////////////////////////////////////////////////////////////////////////////////

        int applyCollectionDumpMarker (struct TRI_transaction_collection_s*,
                                       TRI_replication_operation_e,
                                       const TRI_voc_key_t,
                                       const TRI_voc_rid_t,
                                       struct TRI_json_t const*,
                                       std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

        int createCollection (struct TRI_json_t const*,
                              struct TRI_vocbase_col_s**);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

        int dropCollection (struct TRI_json_t const*,
                            bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

        int createIndex (struct TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

        int dropIndex (struct TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get master state
////////////////////////////////////////////////////////////////////////////////

        int getMasterState (std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the state response of the master
////////////////////////////////////////////////////////////////////////////////

        int handleStateResponse (struct TRI_json_t const*,
                                 std::string&);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase base pointer
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief configuration
////////////////////////////////////////////////////////////////////////////////

        TRI_replication_applier_configuration_t _configuration;

////////////////////////////////////////////////////////////////////////////////
/// @brief information about the master state
////////////////////////////////////////////////////////////////////////////////

        TRI_replication_master_info_t _masterInfo;

////////////////////////////////////////////////////////////////////////////////
/// @brief the update policy object (will be the same for all actions)
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_update_policy_t _policy;

////////////////////////////////////////////////////////////////////////////////
/// @brief the endpoint (master) we're connected to
////////////////////////////////////////////////////////////////////////////////

        rest::Endpoint* _endpoint;

////////////////////////////////////////////////////////////////////////////////
/// @brief the connection to the master
////////////////////////////////////////////////////////////////////////////////

        httpclient::GeneralClientConnection* _connection;

////////////////////////////////////////////////////////////////////////////////
/// @brief the http client we're using
////////////////////////////////////////////////////////////////////////////////

        httpclient::SimpleHttpClient* _client;

////////////////////////////////////////////////////////////////////////////////
/// @brief database name
////////////////////////////////////////////////////////////////////////////////

        std::string _databaseName;

////////////////////////////////////////////////////////////////////////////////
/// @brief local server id
////////////////////////////////////////////////////////////////////////////////

        std::string _localServerIdString;

////////////////////////////////////////////////////////////////////////////////
/// @brief local server id
////////////////////////////////////////////////////////////////////////////////

        TRI_server_id_t _localServerId;

////////////////////////////////////////////////////////////////////////////////
/// @brief base url of the replication API
////////////////////////////////////////////////////////////////////////////////

        static const std::string BaseUrl;
    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
