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
#include "Basics/WorkItem.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

class WorkMonitor : public Thread {
 public:
  WorkMonitor();
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

 public:
  static void freeWorkDescription(WorkDescription* desc);
  static bool pushThread(Thread* thread);
  static void popThread(Thread* thread);
  static void pushAql(TRI_vocbase_t*, uint64_t queryId, char const* text,
                      size_t length);
  static void pushAql(TRI_vocbase_t*, uint64_t queryId);
  static void popAql();
  static void pushCustom(char const* type, char const* text, size_t length);
  static void pushCustom(char const* type, uint64_t id);
  static void popCustom();
  static void pushHandler(rest::RestHandler*);
  static WorkDescription* popHandler(rest::RestHandler*, bool free);
  static void requestWorkOverview(rest::RestHandler* handler);
  static void cancelWork(uint64_t id);

 public:
  static void initialize();
  static void shutdown();

 protected:
  void run() override;

 private:
  static void sendWorkOverview(rest::RestHandler*,
                               std::shared_ptr<velocypack::Buffer<uint8_t>>);
  static bool cancelAql(WorkDescription*);
  static void deleteHandler(WorkDescription* desc);
  static void vpackHandler(velocypack::Builder*, WorkDescription* desc);

  static WorkDescription* createWorkDescription(WorkType);
  static void deleteWorkDescription(WorkDescription*, bool stopped);
  static void activateWorkDescription(WorkDescription*);
  static WorkDescription* deactivateWorkDescription();
  static void vpackWorkDescription(VPackBuilder*, WorkDescription*);
  static void cancelWorkDescriptions(Thread* thread);

 private:
  static std::atomic<bool> stopped;

  static boost::lockfree::queue<WorkDescription*> emptyWorkDescription;
  static boost::lockfree::queue<WorkDescription*> freeableWorkDescription;
  static boost::lockfree::queue<rest::RestHandler*> workOverview;

  static Mutex cancelLock;
  static std::set<uint64_t> cancelIds;

  static Mutex threadsLock;
  static std::set<Thread*> threads;

 private:
  basics::ConditionVariable _waiter;
};

class HandlerWorkStack {
  HandlerWorkStack(const HandlerWorkStack&) = delete;
  HandlerWorkStack& operator=(const HandlerWorkStack&) = delete;

 public:
  // TODO(fc) remove the pointer version
  explicit HandlerWorkStack(rest::RestHandler*);
  explicit HandlerWorkStack(WorkItem::uptr<rest::RestHandler>);

  ~HandlerWorkStack();

 public:
  rest::RestHandler* handler() const { return _handler; }

 private:
  rest::RestHandler* _handler;
};

class AqlWorkStack {
  AqlWorkStack(const AqlWorkStack&) = delete;
  AqlWorkStack& operator=(const AqlWorkStack&) = delete;

 public:
  AqlWorkStack(TRI_vocbase_t*, uint64_t id, char const* text, size_t length);

  AqlWorkStack(TRI_vocbase_t*, uint64_t id);

  ~AqlWorkStack();
};

class CustomWorkStack {
  CustomWorkStack(const CustomWorkStack&) = delete;
  CustomWorkStack& operator=(const CustomWorkStack&) = delete;

 public:
  CustomWorkStack(char const* type, char const* text, size_t length);

  CustomWorkStack(char const* type, uint64_t id);

  ~CustomWorkStack();
};
}
#endif
