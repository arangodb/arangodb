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

#ifndef ARANGOD_MMFILES_SYNCHRONIZER_THREAD_H
#define ARANGOD_MMFILES_SYNCHRONIZER_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "MMFiles/MMFilesWalLogfile.h"

namespace arangodb {
class MMFilesLogfileManager;

class MMFilesSynchronizerThread final : public Thread {
  /// @brief MMFilesSynchronizerThread
 private:
  MMFilesSynchronizerThread(MMFilesSynchronizerThread const&) = delete;
  MMFilesSynchronizerThread& operator=(MMFilesSynchronizerThread const&) = delete;

 public:
  MMFilesSynchronizerThread(MMFilesLogfileManager*, uint64_t);
  ~MMFilesSynchronizerThread() { shutdown(); }

 public:
  void beginShutdown() override final;

 public:
  /// @brief signal that a sync is needed
  void signalSync(bool waitForSync);

 protected:
  void run() override;

 private:
  /// @brief synchronize an unsynchronized region
  int doSync(bool&);

  /// @brief get a logfile descriptor (it caches the descriptor for performance)
  int getLogfileDescriptor(MMFilesWalLogfile::IdType);

 private:
  /// @brief the logfile manager
  MMFilesLogfileManager* _logfileManager;

  /// @brief condition variable for the thread
  basics::ConditionVariable _condition;

  /// @brief wait interval for the synchronizer thread when idle
  uint64_t const _syncInterval;

  /// @brief logfile descriptor cache
  struct {
    MMFilesWalLogfile::IdType id;
    int fd;
  } _logfileCache;

  /// @brief number of requests waiting
  /// the value stored here consists of two parts:
  /// the lower 32 bits contain the number of waiters that requested
  /// a synchronous write, the upper 32 bits contain the number of
  /// waiters that requested asynchronous writes
  std::atomic<uint64_t> _waiting;
};

}  // namespace arangodb

#endif
