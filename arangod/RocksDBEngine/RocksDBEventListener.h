////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_EVENT_LISTENER_H
#define ARANGO_ROCKSDB_ROCKSDB_EVENT_LISTENER_H 1

#include <atomic>
#include <queue>

// public rocksdb headers
#include <rocksdb/listener.h>

#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"

namespace arangodb {

class RocksDBEventListenerThread : public arangodb::Thread {
public:
 explicit RocksDBEventListenerThread(application_features::ApplicationServer& server,
                                     std::string const& name)
     : Thread(server, name) {}
 ~RocksDBEventListenerThread();

 void queueShaCalcFile(std::string const& pathName);

 void queueDeleteFile(std::string const& pathName);

 void signalLoop();

 static bool shaCalcFile(std::string const& filename);

 static bool deleteFile(std::string const& filename);

 static void checkMissingShaFiles(std::string const& pathname, int64_t requireAge);

protected:
 void run() override;

 struct actionNeeded_t {
   enum { CALC_SHA, DELETE_ACTION } _action;
   std::string _path;
  };

  basics::ConditionVariable _loopingCondvar;

  Mutex _pendingMutex;
  std::queue<actionNeeded_t> _pendingQueue;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief The following wrapper routines simplify unit testing
  ////////////////////////////////////////////////////////////////////////////////
  virtual std::string getRocksDBPath();

};// class RocksDBEventListenerThread

////////////////////////////////////////////////////////////////////////////////
///
///
////////////////////////////////////////////////////////////////////////////////
class RocksDBEventListener : public rocksdb::EventListener {
 public:
  explicit RocksDBEventListener(application_features::ApplicationServer&);
  virtual ~RocksDBEventListener();

  void OnFlushCompleted(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnTableFileDeleted(const rocksdb::TableFileDeletionInfo& /*info*/) override;

  void OnCompactionCompleted(rocksdb::DB* db, const rocksdb::CompactionJobInfo& ci) override;

  void beginShutdown() {_shaThread.beginShutdown();};

 protected:

  /// thread to perform sha256 and file deletes in background
  basics::ConditionVariable _threadDone;
  RocksDBEventListenerThread _shaThread;

};  // class RocksDBEventListener

}  // namespace arangodb

#endif
