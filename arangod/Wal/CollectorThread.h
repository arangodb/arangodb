////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log garbage collection thread
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_WAL_COLLECTOR_THREAD_H
#define ARANGODB_WAL_COLLECTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "VocBase/barrier.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-types.h"
#include "Wal/Logfile.h"

struct TRI_datafile_s;
struct TRI_df_marker_s;
struct TRI_document_collection_t;
struct TRI_server_s;

namespace triagens {
  namespace wal {

    class LogfileManager;
    class Logfile;

// -----------------------------------------------------------------------------
// --SECTION--                                         struct CollectorOperation
// -----------------------------------------------------------------------------

    struct CollectorOperation {
      CollectorOperation (char const* datafilePosition,
                          TRI_voc_size_t datafileMarkerSize,
                          char const* walPosition,
                          TRI_voc_fid_t datafileId)
        : datafilePosition(datafilePosition),
          datafileMarkerSize(datafileMarkerSize),
          walPosition(walPosition),
          datafileId(datafileId) {
        TRI_ASSERT_EXPENSIVE(datafilePosition != nullptr);
        TRI_ASSERT_EXPENSIVE(datafileMarkerSize > 0);
        TRI_ASSERT_EXPENSIVE(walPosition != nullptr);
        TRI_ASSERT_EXPENSIVE(datafileId > 0);
      }

      char const*     datafilePosition;
      TRI_voc_size_t  datafileMarkerSize;
      char const*     walPosition;
      TRI_voc_fid_t   datafileId;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                             struct CollectorCache
// -----------------------------------------------------------------------------

    struct CollectorCache {
      CollectorCache (CollectorCache const&) = delete;
      CollectorCache& operator= (CollectorCache const&) = delete;

      explicit CollectorCache (TRI_voc_cid_t collectionId,
                               TRI_voc_tick_t databaseId,
                               Logfile* logfile,
                               int64_t totalOperationsCount,
                               size_t operationsSize)
        : collectionId(collectionId),
          databaseId(databaseId),
          logfile(logfile),
          totalOperationsCount(totalOperationsCount),
          operations(new std::vector<CollectorOperation>()),
          barriers(),
          dfi(),
          lastFid(0),
          lastDatafile(nullptr) {

        operations->reserve(operationsSize);
      }

      ~CollectorCache () {
        if (operations != nullptr) {
          delete operations;
        }
        freeBarriers();
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a barrier
////////////////////////////////////////////////////////////////////////////////

      void addBarrier (TRI_barrier_t* barrier) {
        TRI_ASSERT(barrier != nullptr);
        barriers.push_back(barrier);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief free all barriers
////////////////////////////////////////////////////////////////////////////////

      void freeBarriers () {
        for (auto it = barriers.begin(); it != barriers.end(); ++it) {
          TRI_FreeBarrier((*it));
        }

        barriers.clear();
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief id of collection
////////////////////////////////////////////////////////////////////////////////

      TRI_voc_cid_t const collectionId;

////////////////////////////////////////////////////////////////////////////////
/// @brief id of database
////////////////////////////////////////////////////////////////////////////////

      TRI_voc_tick_t const databaseId;

////////////////////////////////////////////////////////////////////////////////
/// @brief id of the WAL logfile
////////////////////////////////////////////////////////////////////////////////

      Logfile* logfile;

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of operations in this block
////////////////////////////////////////////////////////////////////////////////

      int64_t const totalOperationsCount;

////////////////////////////////////////////////////////////////////////////////
/// @brief all collector operations of a collection
////////////////////////////////////////////////////////////////////////////////

      std::vector<CollectorOperation>* operations;

////////////////////////////////////////////////////////////////////////////////
/// @brief barriers held by the operations
////////////////////////////////////////////////////////////////////////////////

      std::vector<TRI_barrier_t*> barriers;

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile info cache, updated when the collector transfers markers
////////////////////////////////////////////////////////////////////////////////

      std::unordered_map<TRI_voc_fid_t, TRI_doc_datafile_info_t> dfi;

////////////////////////////////////////////////////////////////////////////////
/// @brief id of last datafile handled
////////////////////////////////////////////////////////////////////////////////

      TRI_voc_fid_t lastFid;

////////////////////////////////////////////////////////////////////////////////
/// @brief last datafile written to
////////////////////////////////////////////////////////////////////////////////

      TRI_datafile_t* lastDatafile;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                             class CollectorThread
// -----------------------------------------------------------------------------

    class CollectorThread : public basics::Thread {

////////////////////////////////////////////////////////////////////////////////
/// @brief CollectorThread
////////////////////////////////////////////////////////////////////////////////

      private:
        CollectorThread (CollectorThread const&) = delete;
        CollectorThread& operator= (CollectorThread const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the collector thread
////////////////////////////////////////////////////////////////////////////////

        CollectorThread (LogfileManager*,
                         struct TRI_server_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the collector thread
////////////////////////////////////////////////////////////////////////////////

        ~CollectorThread ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   public typedefs
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef key => document marker
////////////////////////////////////////////////////////////////////////////////

        typedef std::unordered_map<std::string, struct TRI_df_marker_s const*> DocumentOperationsType;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for structural operation (attributes, shapes) markers
////////////////////////////////////////////////////////////////////////////////

        typedef std::vector<struct TRI_df_marker_s const*> OperationsType;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the collector thread
////////////////////////////////////////////////////////////////////////////////

        void stop ();

////////////////////////////////////////////////////////////////////////////////
/// @brief signal the thread that there is something to do
////////////////////////////////////////////////////////////////////////////////

        void signal ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

        void run ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether there are queued operations left
////////////////////////////////////////////////////////////////////////////////

        bool hasQueuedOperations ();

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of queued operations
////////////////////////////////////////////////////////////////////////////////

        size_t numQueuedOperations ();

////////////////////////////////////////////////////////////////////////////////
/// @brief step 1: perform collection of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

        bool collectLogfiles ();

////////////////////////////////////////////////////////////////////////////////
/// @brief step 2: process all still-queued collection operations
////////////////////////////////////////////////////////////////////////////////

        bool processQueuedOperations ();

////////////////////////////////////////////////////////////////////////////////
/// @brief process all operations for a single collection
////////////////////////////////////////////////////////////////////////////////

        int processCollectionOperations (CollectorCache*);

////////////////////////////////////////////////////////////////////////////////
/// @brief collect one logfile
////////////////////////////////////////////////////////////////////////////////

        int collect (Logfile*);

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer markers into a collection
////////////////////////////////////////////////////////////////////////////////

        int transferMarkers (triagens::wal::Logfile*,
                             TRI_voc_cid_t,
                             TRI_voc_tick_t,
                             int64_t,
                             OperationsType const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer markers into a collection
////////////////////////////////////////////////////////////////////////////////

        int executeTransferMarkers (TRI_document_collection_t*,
                                    CollectorCache*,
                                    OperationsType const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert the collect operations into a per-collection queue
////////////////////////////////////////////////////////////////////////////////

        int queueOperations (triagens::wal::Logfile*,
                             CollectorCache*&);

////////////////////////////////////////////////////////////////////////////////
/// @brief update a collection's datafile information
////////////////////////////////////////////////////////////////////////////////

        int updateDatafileStatistics (TRI_document_collection_t*,
                                      CollectorCache*);

////////////////////////////////////////////////////////////////////////////////
/// @brief sync the journals of a collection
////////////////////////////////////////////////////////////////////////////////

        int syncDatafileCollection (struct TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next free position for a new marker of the specified size
////////////////////////////////////////////////////////////////////////////////

        char* nextFreeMarkerPosition (struct TRI_document_collection_t*,
                                      TRI_voc_tick_t,
                                      TRI_df_marker_type_e,
                                      TRI_voc_size_t,
                                      CollectorCache*);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a marker
////////////////////////////////////////////////////////////////////////////////

        void initMarker (struct TRI_df_marker_s*,
                         TRI_df_marker_type_e,
                         TRI_voc_size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the tick of a marker and calculate its CRC value
////////////////////////////////////////////////////////////////////////////////

        void finishMarker (char const*,
                           char*,
                           struct TRI_document_collection_t*,
                           TRI_voc_tick_t,
                           CollectorCache*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile manager
////////////////////////////////////////////////////////////////////////////////

        LogfileManager* _logfileManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to the server
////////////////////////////////////////////////////////////////////////////////

        struct TRI_server_s* _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable for the collector thread
////////////////////////////////////////////////////////////////////////////////

        basics::ConditionVariable _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief operations lock
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Mutex _operationsQueueLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief operations to collect later
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<TRI_voc_cid_t, std::vector<CollectorCache*>> _operationsQueue;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the queue is currently in use
////////////////////////////////////////////////////////////////////////////////

        bool _operationsQueueInUse;

////////////////////////////////////////////////////////////////////////////////
/// @brief stop flag
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _stop;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of pending operations in collector queue
////////////////////////////////////////////////////////////////////////////////

        uint64_t _numPendingOperations;

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the collector thread when idle
////////////////////////////////////////////////////////////////////////////////

        static uint64_t const Interval;

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
