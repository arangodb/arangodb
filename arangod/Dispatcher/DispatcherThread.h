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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_DISPATCHER_DISPATCHER_THREAD_H
#define ARANGOD_DISPATCHER_DISPATCHER_THREAD_H 1

#include "Basics/Thread.h"

namespace arangodb {
namespace rest {
class DispatcherQueue;
class Job;

/////////////////////////////////////////////////////////////////////////////
/// @brief job dispatcher thread
/////////////////////////////////////////////////////////////////////////////

class DispatcherThread : public Thread {
  DispatcherThread(DispatcherThread const&) = delete;
  DispatcherThread& operator=(DispatcherThread const&) = delete;

  friend class Dispatcher;
  friend class DispatcherQueue;

 public:
  static DispatcherThread* current() {
    return dynamic_cast<DispatcherThread*>(Thread::CURRENT_THREAD);
  }

 public:
  explicit DispatcherThread(DispatcherQueue*);
  ~DispatcherThread() {shutdown();}

 protected:
  void run() override;

  void addStatus(arangodb::velocypack::Builder* b) override;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief indicates that thread is doing a blocking operation
  //////////////////////////////////////////////////////////////////////////////

  void block();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief indicates that thread has resumed work
  //////////////////////////////////////////////////////////////////////////////

  void unblock();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief do the real work
  //////////////////////////////////////////////////////////////////////////////

  void handleJob(Job* job);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the dispatcher queue
  //////////////////////////////////////////////////////////////////////////////

  DispatcherQueue* _queue;
};
}
}

#endif
