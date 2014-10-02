////////////////////////////////////////////////////////////////////////////////
/// @brief replication initial data synchroniser
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

#ifndef ARANGODB_REPLICATION_INITIAL_SYNCER_H
#define ARANGODB_REPLICATION_INITIAL_SYNCER_H 1

#include "Basics/Common.h"

#include "Replication/Syncer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_json_t;
struct TRI_replication_applier_configuration_s;
struct TRI_transaction_collection_s;
struct TRI_vocbase_s;

namespace triagens {

  namespace httpclient {
    class SimpleHttpResult;
  }

  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                     InitialSyncer
// -----------------------------------------------------------------------------

    class InitialSyncer : public Syncer {

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief apply phases
////////////////////////////////////////////////////////////////////////////////

        typedef enum {
          PHASE_NONE,
          PHASE_INIT,
          PHASE_VALIDATE,
          PHASE_DROP,
          PHASE_CREATE,
          PHASE_DUMP
        }
        sync_phase_e;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        InitialSyncer (struct TRI_vocbase_s*,
                       struct TRI_replication_applier_configuration_s const*,
                       std::map<std::string, bool> const&,
                       std::string const&,
                       bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~InitialSyncer ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief run method, performs a full synchronisation
////////////////////////////////////////////////////////////////////////////////

        int run (std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the last log tick of the master at start
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_tick_t getLastLogTick () const {
          return _masterInfo._lastLogTick;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collections that were synced
////////////////////////////////////////////////////////////////////////////////

        std::map<TRI_voc_cid_t, std::string> const& getProcessedCollections () const {
          return _processedCollections;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief set a progress message
////////////////////////////////////////////////////////////////////////////////

        void setProgress (const std::string& message) {
          _progress = message;

          if (_verbose) {
            LOG_INFO("synchronisation progress: %s", message.c_str());
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "start batch" command
////////////////////////////////////////////////////////////////////////////////

        int sendStartBatch (std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief send an "extend batch" command
////////////////////////////////////////////////////////////////////////////////

        int sendExtendBatch ();

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "finish batch" command
////////////////////////////////////////////////////////////////////////////////

        int sendFinishBatch ();

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump
////////////////////////////////////////////////////////////////////////////////

        int applyCollectionDump (struct TRI_transaction_collection_s*,
                                 httpclient::SimpleHttpResult*,
                                 std::string&);


////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

        int handleCollectionDump (struct TRI_transaction_collection_s*,
                                  std::string const&,
                                  TRI_voc_tick_t,
                                  std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the information about a collection
////////////////////////////////////////////////////////////////////////////////

        int handleCollection (struct TRI_json_t const*,
                              struct TRI_json_t const*,
                              std::string&,
                              sync_phase_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the inventory response of the master
////////////////////////////////////////////////////////////////////////////////

        int handleInventoryResponse (struct TRI_json_t const*,
                                     std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all collections from a list and apply an action
////////////////////////////////////////////////////////////////////////////////

        int iterateCollections (struct TRI_json_t const*,
                                std::string&,
                                sync_phase_e);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief progress message
////////////////////////////////////////////////////////////////////////////////

        std::string _progress;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection restriction
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, bool> _restrictCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection restriction type
////////////////////////////////////////////////////////////////////////////////

        std::string const _restrictType;

////////////////////////////////////////////////////////////////////////////////
/// @brief collections synced
////////////////////////////////////////////////////////////////////////////////

        std::map<TRI_voc_cid_t, std::string> _processedCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief dump batch id
////////////////////////////////////////////////////////////////////////////////

        uint64_t _batchId;

////////////////////////////////////////////////////////////////////////////////
/// @brief dump batch last update time
////////////////////////////////////////////////////////////////////////////////

        double _batchUpdateTime;

////////////////////////////////////////////////////////////////////////////////
/// @brief ttl for batches
////////////////////////////////////////////////////////////////////////////////

        int _batchTtl;

////////////////////////////////////////////////////////////////////////////////
/// @brief chunk size to use
////////////////////////////////////////////////////////////////////////////////

        std::string _chunkSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief verbosity
////////////////////////////////////////////////////////////////////////////////

        bool _verbose;

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
