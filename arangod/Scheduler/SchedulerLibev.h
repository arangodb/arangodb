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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SCHEDULER_LIBEV_H
#define ARANGOD_SCHEDULER_SCHEDULER_LIBEV_H 1

#include "Basics/Common.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief input-output scheduler using libev
////////////////////////////////////////////////////////////////////////////////

class SchedulerLibev : public Scheduler {
 private:
  SchedulerLibev(SchedulerLibev const&);
  SchedulerLibev& operator=(SchedulerLibev const&);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the available backends
  //////////////////////////////////////////////////////////////////////////////

  static int availableBackends();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief switch the libev allocator
  //////////////////////////////////////////////////////////////////////////////

  static void switchAllocator();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a scheduler
  //////////////////////////////////////////////////////////////////////////////

  explicit SchedulerLibev(size_t nrThreads = 1, int backend = BACKEND_AUTO);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief deletes a scheduler
  //////////////////////////////////////////////////////////////////////////////

  ~SchedulerLibev();

 public:
  void eventLoop(EventLoop) override;

  void wakeupLoop(EventLoop) override;

  EventToken installSocketEvent(EventLoop, EventType, Task*,
                                TRI_socket_t) override;

  void startSocketEvents(EventToken) override;

  void stopSocketEvents(EventToken) override;

  EventToken installTimerEvent(EventLoop, Task*, double timeout) override;

  void clearTimer(EventToken) override;

  void rearmTimer(EventToken, double timeout) override;

  EventToken installPeriodicEvent(EventLoop, Task*, double offset,
                                  double interval) override;

  void rearmPeriodic(EventToken, double offset, double timeout) override;

  EventToken installSignalEvent(EventLoop, Task*, int signal) override;

  void uninstallEvent(EventToken) override;

  void signalTask(std::unique_ptr<TaskData>&) override;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up an event lookup
  //////////////////////////////////////////////////////////////////////////////

  void* lookupLoop(EventLoop);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief backend to use
  //////////////////////////////////////////////////////////////////////////////

  int _backend;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief event loops
  //////////////////////////////////////////////////////////////////////////////

  void* _loops;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief event wakers
  //////////////////////////////////////////////////////////////////////////////

  void* _wakers;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the allocator was switched
  //////////////////////////////////////////////////////////////////////////////

  static bool SwitchedAllocator;
};
}
}

#endif
