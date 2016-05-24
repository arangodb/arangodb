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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_WAL_COLLECTOR_THREAD_H
#define ARANGOD_WAL_COLLECTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/datafile.h"
#include "VocBase/DatafileStatistics.h"
#include "VocBase/Ditch.h"
#include "VocBase/voc-types.h"
#include "Wal/Logfile.h"

struct TRI_datafile_t;
struct TRI_df_marker_t;
struct TRI_document_collection_t;
struct TRI_server_t;

namespace arangodb {
namespace wal {

class LogfileManager;
class Logfile;

struct CollectorOperation {
  CollectorOperation(char const* datafilePosition,
                     TRI_voc_size_t datafileMarkerSize, char const* walPosition,
                     TRI_voc_fid_t datafileId)
      : datafilePosition(datafilePosition),
        datafileMarkerSize(datafileMarkerSize),
        walPosition(walPosition),
        datafileId(datafileId) {
    TRI_ASSERT(datafilePosition != nullptr);
    TRI_ASSERT(datafileMarkerSize > 0);
    TRI_ASSERT(walPosition != nullptr);
    TRI_ASSERT(datafileId > 0);
  }

  char const* datafilePosition;
  TRI_voc_size_t datafileMarkerSize;
  char const* walPosition;
  TRI_voc_fid_t datafileId;
};

struct CollectorCache {
  CollectorCache(CollectorCache const&) = delete;
  CollectorCache& operator=(CollectorCache const&) = delete;

  CollectorCache(TRI_voc_cid_t collectionId, TRI_voc_tick_t databaseId,
                 Logfile* logfile, int64_t totalOperationsCount,
                 size_t operationsSize)
      : collectionId(collectionId),
        databaseId(databaseId),
        logfile(logfile),
        totalOperationsCount(totalOperationsCount),
        operations(new std::vector<CollectorOperation>()),
        ditches(),
        dfi(),
        lastFid(0),
        lastDatafile(nullptr) {
    operations->reserve(operationsSize);
  }

  ~CollectorCache() {
    if (operations != nullptr) {
      delete operations;
    }
    freeDitches();
  }

  /// @brief add a ditch
  void addDitch(arangodb::DocumentDitch* ditch) {
    TRI_ASSERT(ditch != nullptr);
    ditches.emplace_back(ditch);
  }

  /// @brief free all ditches
  void freeDitches() {
    for (auto& it : ditches) {
      it->ditches()->freeDocumentDitch(it, false);
    }

    ditches.clear();
  }

  /// @brief id of collection
  TRI_voc_cid_t const collectionId;

  /// @brief id of database
  TRI_voc_tick_t const databaseId;

  /// @brief id of the WAL logfile
  Logfile* logfile;

  /// @brief total number of operations in this block
  int64_t const totalOperationsCount;

  /// @brief all collector operations of a collection
  std::vector<CollectorOperation>* operations;

  /// @brief ditches held by the operations
  std::vector<arangodb::DocumentDitch*> ditches;

  /// @brief datafile info cache, updated when the collector transfers markers
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer> dfi;

  /// @brief id of last datafile handled
  TRI_voc_fid_t lastFid;

  /// @brief last datafile written to
  TRI_datafile_t* lastDatafile;
};

class CollectorThread : public Thread {
  CollectorThread(CollectorThread const&) = delete;
  CollectorThread& operator=(CollectorThread const&) = delete;

 public:
  CollectorThread(LogfileManager*, TRI_server_t*);
  ~CollectorThread() { shutdown(); }

 public:
  /// @brief typedef key => document marker
  typedef std::unordered_map<std::string, struct TRI_df_marker_t const*>
      DocumentOperationsType;

  /// @brief typedef for structural operation (attributes, shapes) markers
  typedef std::vector<struct TRI_df_marker_t const*> OperationsType;

 public:
  void beginShutdown() override final;

 public:
  /// @brief wait for the collector result
  int waitForResult(uint64_t);

  /// @brief signal the thread that there is something to do
  void signal();

  /// @brief check whether there are queued operations left
  bool hasQueuedOperations();

  /// @brief check whether there are queued operations left for the given
  /// collection
  bool hasQueuedOperations(TRI_voc_cid_t);

 protected:
  /// @brief main loop
  void run() override;

 private:
  /// @brief process a single marker in collector step 2
  void processCollectionMarker(
      arangodb::SingleCollectionTransaction&,
      TRI_document_collection_t*, CollectorCache*, CollectorOperation const&);

  /// @brief return the number of queued operations
  size_t numQueuedOperations();

  /// @brief step 1: perform collection of a logfile (if any)
  int collectLogfiles(bool&);

  /// @brief step 2: process all still-queued collection operations
  int processQueuedOperations(bool&);

  /// @brief process all operations for a single collection
  int processCollectionOperations(CollectorCache*);

  /// @brief collect one logfile
  int collect(Logfile*);

  /// @brief transfer markers into a collection
  int transferMarkers(arangodb::wal::Logfile*, TRI_voc_cid_t, TRI_voc_tick_t,
                      int64_t, OperationsType const&);

  /// @brief transfer markers into a collection
  int executeTransferMarkers(TRI_document_collection_t*, CollectorCache*,
                             OperationsType const&);

  /// @brief insert the collect operations into a per-collection queue
  int queueOperations(arangodb::wal::Logfile*, CollectorCache*&);

  /// @brief update a collection's datafile information
  int updateDatafileStatistics(TRI_document_collection_t*, CollectorCache*);

  /// @brief sync the journal of a collection
  int syncJournalCollection(struct TRI_document_collection_t*);

  /// @brief get the next free position for a new marker of the specified size
  char* nextFreeMarkerPosition(struct TRI_document_collection_t*,
                               TRI_voc_tick_t, TRI_df_marker_type_t,
                               TRI_voc_size_t, CollectorCache*);

  /// @brief set the tick of a marker and calculate its CRC value
  void finishMarker(char const*, char*, struct TRI_document_collection_t*,
                    TRI_voc_tick_t, CollectorCache*);

 private:
  /// @brief the logfile manager
  LogfileManager* _logfileManager;

  /// @brief pointer to the server
  TRI_server_t* _server;

  /// @brief condition variable for the collector thread
  basics::ConditionVariable _condition;

  /// @brief operations lock
  arangodb::Mutex _operationsQueueLock;

  /// @brief operations to collect later
  std::unordered_map<TRI_voc_cid_t, std::vector<CollectorCache*>>
      _operationsQueue;

  /// @brief whether or not the queue is currently in use
  bool _operationsQueueInUse;

  /// @brief number of pending operations in collector queue
  uint64_t _numPendingOperations;

  /// @brief condition variable for the collector thread result
  basics::ConditionVariable _collectorResultCondition;

  /// @brief last collector result
  int _collectorResult;

  /// @brief wait interval for the collector thread when idle
  static uint64_t const Interval;
};
}
}

#endif
