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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_WORK_MONITOR_H
#define ARANGODB_BASICS_WORK_MONITOR_H 1

#include "Basics/Thread.h"

#include <boost/lockfree/queue.hpp>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/WorkDescription.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       WorkMonitor
// -----------------------------------------------------------------------------

class WorkMonitor : public Thread {
 public:
  static void freeWorkDescription(WorkDescription* desc);
  static bool pushThread(Thread* thread);
  static void popThread(Thread* thread);
  static void pushAql(TRI_vocbase_t*, uint64_t queryId, char const* text, size_t length);
  static void pushAql(TRI_vocbase_t*, uint64_t queryId);
  static void popAql();
  static void pushCustom(char const* type, char const* text, size_t length);
  static void pushCustom(char const* type, uint64_t id);
  static void popCustom();
  static void pushHandler(std::shared_ptr<rest::RestHandler>);
  static void popHandler();
  static void requestWorkOverview(std::shared_ptr<rest::RestHandler>,
                                  std::function<void()> next);
  static void cancelWork(uint64_t id);

 public:
  static void initialize();
  static void shutdown();
  static void clearHandlers();
  static bool clearWorkDescriptions();

 private:
  static WorkDescription* createWorkDescription(WorkType);
  static void deleteWorkDescription(WorkDescription*, bool stopped);
  static void activateWorkDescription(WorkDescription*);
  static WorkDescription* deactivateWorkDescription();
  static void vpackWorkDescription(VPackBuilder*, WorkDescription*);
  static void cancelWorkDescriptions(Thread* thread);

  // implemented in WorkMonitorArangod.cpp
  static void addWorkOverview(std::shared_ptr<rest::RestHandler>,
                              std::shared_ptr<velocypack::Buffer<uint8_t>>);
  static bool cancelAql(WorkDescription*);
  static void deleteHandler(WorkDescription* desc);
  static void vpackHandler(velocypack::Builder*, WorkDescription* desc);

 private:
  static std::atomic<bool> _stopped;

  static boost::lockfree::queue<WorkDescription*> _emptyWorkDescription;
  static boost::lockfree::queue<WorkDescription*> _freeableWorkDescription;
  static boost::lockfree::queue<std::pair<std::shared_ptr<rest::RestHandler>, std::function<void()>>*> _workOverview;

  static Mutex _cancelLock;
  static std::set<uint64_t> _cancelIds;

  static Mutex _threadsLock;
  static std::set<Thread*> _threads;

 public:
  WorkMonitor() : Thread("WorkMonitor") {}

  ~WorkMonitor() {
    if (hasStarted()) {
      Thread::shutdown();
    }
  }

 public:
  bool isSilent() override final { return true; }

  void beginShutdown() override final {
    Thread::beginShutdown();
    _waiter.broadcast();
  }

 protected:
  void run() override;

 private:
  void clearAllHandlers();

 private:
  basics::ConditionVariable _waiter;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  HandlerWorkStack
// -----------------------------------------------------------------------------

class HandlerWorkStack {
  HandlerWorkStack(const HandlerWorkStack&) = delete;
  HandlerWorkStack& operator=(const HandlerWorkStack&) = delete;

 public:
  explicit HandlerWorkStack(std::shared_ptr<rest::RestHandler>);
  ~HandlerWorkStack();

 public:
  rest::RestHandler* handler() const { return _handler.get(); }
  rest::RestHandler* operator->() { return _handler.get(); }

 private:
  std::shared_ptr<rest::RestHandler> _handler;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      AqlWorkStack
// -----------------------------------------------------------------------------

class AqlWorkStack {
  AqlWorkStack(const AqlWorkStack&) = delete;
  AqlWorkStack& operator=(const AqlWorkStack&) = delete;

 public:
  AqlWorkStack(TRI_vocbase_t* vocbase, uint64_t queryId, char const* text, size_t length) {
    WorkMonitor::pushAql(vocbase, queryId, text, length);
  }

  AqlWorkStack(TRI_vocbase_t* vocbase, uint64_t queryId) {
    WorkMonitor::pushAql(vocbase, queryId);
  }

  ~AqlWorkStack() { WorkMonitor::popAql(); }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  CustomeWorkStack
// -----------------------------------------------------------------------------

class CustomWorkStack {
  CustomWorkStack(const CustomWorkStack&) = delete;
  CustomWorkStack& operator=(const CustomWorkStack&) = delete;

 public:
  CustomWorkStack(char const* type, char const* text, size_t length) {
    WorkMonitor::pushCustom(type, text, length);
  }

  CustomWorkStack(char const* type, uint64_t id) {
    WorkMonitor::pushCustom(type, id);
  }

  ~CustomWorkStack() { WorkMonitor::popCustom(); }
};
}  // namespace arangodb

#endif
