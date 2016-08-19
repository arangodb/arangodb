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

#include "WorkMonitor.h"

#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "Basics/tri-strings.h"

using namespace arangodb;
using namespace arangodb::basics;

std::atomic<bool> WorkMonitor::stopped(true);

boost::lockfree::queue<WorkDescription*> WorkMonitor::emptyWorkDescription(128);
boost::lockfree::queue<WorkDescription*> WorkMonitor::freeableWorkDescription(
    128);
boost::lockfree::queue<rest::RestHandler*> WorkMonitor::workOverview(128);

Mutex WorkMonitor::cancelLock;
std::set<uint64_t> WorkMonitor::cancelIds;

Mutex WorkMonitor::threadsLock;
std::set<Thread*> WorkMonitor::threads;

////////////////////////////////////////////////////////////////////////////////
/// @brief singleton
////////////////////////////////////////////////////////////////////////////////

static WorkMonitor WORK_MONITOR;

////////////////////////////////////////////////////////////////////////////////
/// @brief current work description as thread local variable
////////////////////////////////////////////////////////////////////////////////

static thread_local WorkDescription* CURRENT_WORK_DESCRIPTION = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief vpack representation of a work description
////////////////////////////////////////////////////////////////////////////////

namespace {
std::atomic<uint64_t> NEXT_DESC_ID(static_cast<uint64_t>(0));
}

WorkDescription::WorkDescription(WorkType type, WorkDescription* prev)
    : _type(type), _prev(prev), _id(0), _destroy(true), _canceled(false) {}

WorkMonitor::WorkMonitor() : Thread("WorkMonitor") {}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a work description
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::freeWorkDescription(WorkDescription* desc) {
  if (stopped.load()) {
    deleteWorkDescription(desc, true);
  } else {
    freeableWorkDescription.push(desc);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes a thread
////////////////////////////////////////////////////////////////////////////////

bool WorkMonitor::pushThread(Thread* thread) {
  if (stopped.load()) {
    return false;
  }

  TRI_ASSERT(thread != nullptr);
  TRI_ASSERT(Thread::CURRENT_THREAD == nullptr);
  Thread::CURRENT_THREAD = thread;

  try {
    WorkDescription* desc = createWorkDescription(WorkType::THREAD);
    desc->_data.thread = thread;

    activateWorkDescription(desc);

    {
      MUTEX_LOCKER(guard, threadsLock);
      threads.insert(thread);
    }
  } catch (...) {
    throw;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops a thread
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::popThread(Thread* thread) {
  if (stopped.load()) {
    return;
  }

  TRI_ASSERT(thread != nullptr);
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc->_type == WorkType::THREAD);
  TRI_ASSERT(desc->_data.thread == thread);

  try {
    freeWorkDescription(desc);

    {
      MUTEX_LOCKER(guard, threadsLock);
      threads.erase(thread);
    }
  } catch (...) {
    // just to prevent throwing exceptions from here, as this method
    // will be called in destructors...
  }
}

void WorkMonitor::pushAql(TRI_vocbase_t* vocbase, uint64_t queryId,
                          char const* text, size_t length) {
  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(text != nullptr);

  WorkDescription* desc = createWorkDescription(WorkType::AQL_STRING);
  TRI_ASSERT(desc != nullptr);

  desc->_identifier._id = queryId;
  desc->_vocbase = vocbase;

  if (sizeof(desc->_data.text) - 1 < length) {
    length = sizeof(desc->_data.text) - 1;
  }

  TRI_CopyString(desc->_data.text, text, length);

  activateWorkDescription(desc);
}

void WorkMonitor::pushAql(TRI_vocbase_t* vocbase, uint64_t queryId) {
  TRI_ASSERT(vocbase != nullptr);

  WorkDescription* desc = createWorkDescription(WorkType::AQL_ID);
  TRI_ASSERT(desc != nullptr);

  desc->_identifier._id = queryId;
  desc->_vocbase = vocbase;

  activateWorkDescription(desc);
}

void WorkMonitor::popAql() {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc != nullptr);
  TRI_ASSERT(desc->_type == WorkType::AQL_STRING ||
             desc->_type == WorkType::AQL_ID);

  try {
    freeWorkDescription(desc);
  } catch (...) {
    // just to prevent throwing exceptions from here, as this method
    // will be called in destructors...
  }
}

void WorkMonitor::pushCustom(char const* type, char const* text,
                             size_t length) {
  TRI_ASSERT(type != nullptr);
  TRI_ASSERT(text != nullptr);

  WorkDescription* desc = createWorkDescription(WorkType::CUSTOM);
  TRI_ASSERT(desc != nullptr);

  TRI_CopyString(desc->_identifier._customType, type,
                 sizeof(desc->_identifier._customType) - 1);

  if (sizeof(desc->_data.text) - 1 < length) {
    length = sizeof(desc->_data.text) - 1;
  }

  TRI_CopyString(desc->_data.text, text, length);

  activateWorkDescription(desc);
}

void WorkMonitor::pushCustom(char const* type, uint64_t id) {
  TRI_ASSERT(type != nullptr);

  WorkDescription* desc = createWorkDescription(WorkType::CUSTOM);
  TRI_ASSERT(desc != nullptr);

  TRI_CopyString(desc->_identifier._customType, type,
                 sizeof(desc->_identifier._customType) - 1);

  std::string idString(std::to_string(id));
  TRI_CopyString(desc->_data.text, idString.c_str(), idString.size());

  activateWorkDescription(desc);
}

void WorkMonitor::popCustom() {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc != nullptr);
  TRI_ASSERT(desc->_type == WorkType::CUSTOM);

  try {
    freeWorkDescription(desc);
  } catch (...) {
    // just to prevent throwing exceptions from here, as this method
    // will be called in destructors...
  }
}

void WorkMonitor::requestWorkOverview(rest::RestHandler* handler) {
  workOverview.push(handler);
}

void WorkMonitor::cancelWork(uint64_t id) {
  MUTEX_LOCKER(guard, cancelLock);
  cancelIds.insert(id);
}

WorkDescription* WorkMonitor::createWorkDescription(WorkType type) {
  WorkDescription* desc = nullptr;
  WorkDescription* prev = (Thread::CURRENT_THREAD == nullptr)
                              ? CURRENT_WORK_DESCRIPTION
                              : Thread::CURRENT_THREAD->workDescription();

  if (emptyWorkDescription.pop(desc) && desc != nullptr) {
    desc->_type = type;
    desc->_prev.store(prev);
    desc->_destroy = true;
  } else {
    desc = new WorkDescription(type, prev);
  }

  desc->_id = NEXT_DESC_ID.fetch_add(1, std::memory_order_seq_cst);
  desc->_data.handler = nullptr;

  return desc;
}

void WorkMonitor::deleteWorkDescription(WorkDescription* desc, bool stopped) {
  if (desc->_destroy) {
    switch (desc->_type) {
      case WorkType::THREAD:
      case WorkType::CUSTOM:
      case WorkType::AQL_ID:
      case WorkType::AQL_STRING:
        break;

      case WorkType::HANDLER:
        deleteHandler(desc);
        break;
    }
  }

  if (stopped) {
    // we'll be getting here if the work monitor thread is already shut down
    // and cannot delete anything anymore. this means we ourselves are
    // responsible for cleaning up!
    delete desc;
    return;
  }

  // while the work monitor thread is still active, push the item on the
  // work monitor's cleanup list for destruction
  emptyWorkDescription.push(desc);
}

void WorkMonitor::activateWorkDescription(WorkDescription* desc) {
  if (Thread::CURRENT_THREAD == nullptr) {
    CURRENT_WORK_DESCRIPTION = desc;
  } else {
    Thread::CURRENT_THREAD->setWorkDescription(desc);
  }
}

WorkDescription* WorkMonitor::deactivateWorkDescription() {
  if (Thread::CURRENT_THREAD == nullptr) {
    WorkDescription* desc = CURRENT_WORK_DESCRIPTION;
    TRI_ASSERT(desc != nullptr);
    CURRENT_WORK_DESCRIPTION = desc->_prev.load();
    return desc;
  } else {
    return Thread::CURRENT_THREAD->setPrevWorkDescription();
  }
}

void WorkMonitor::vpackWorkDescription(VPackBuilder* b, WorkDescription* desc) {
  switch (desc->_type) {
    case WorkType::THREAD:
      b->add("type", VPackValue("thread"));
      b->add("name", VPackValue(desc->_data.thread->name()));
      b->add("number", VPackValue(desc->_data.thread->threadNumber()));
      b->add("status", VPackValue(VPackValueType::Object));
      desc->_data.thread->addStatus(b);
      b->close();
      break;

    case WorkType::CUSTOM:
      b->add("type", VPackValue(desc->_identifier._customType));
      b->add("description", VPackValue(desc->_data.text));
      break;

    case WorkType::AQL_STRING:
      b->add("type", VPackValue("AQL query"));
      b->add("queryId", VPackValue(desc->_identifier._id));
      b->add("description", VPackValue(desc->_data.text));
      break;

    case WorkType::AQL_ID:
      b->add("type", VPackValue("AQL query id"));
      b->add("queryId", VPackValue(desc->_identifier._id));
      break;

    case WorkType::HANDLER:
      vpackHandler(b, desc);
      break;
  }

  b->add("id", VPackValue(desc->_id));

  auto prev = desc->_prev.load();

  if (prev != nullptr) {
    b->add("parent", VPackValue(VPackValueType::Object));
    vpackWorkDescription(b, prev);
    b->close();
  }
}

void WorkMonitor::cancelWorkDescriptions(Thread* thread) {
  WorkDescription* desc = thread->workDescription();

  std::vector<WorkDescription*> path;

  while (desc != nullptr && desc->_type != WorkType::THREAD) {
    path.push_back(desc);

    uint64_t id = desc->_id;

    if (cancelIds.find(id) != cancelIds.end()) {
      for (auto it = path.rbegin(); it < path.rend(); ++it) {
        bool descent = true;
        WorkDescription* d = *it;

        switch (d->_type) {
          case WorkType::THREAD:
          case WorkType::CUSTOM:
          case WorkType::HANDLER:
            d->_canceled.store(true);
            break;

          case WorkType::AQL_STRING:
          case WorkType::AQL_ID:
            descent = cancelAql(d);
            break;
        }

        if (!descent) {
          break;
        }
      }

      return;
    }

    desc = desc->_prev.load();
  }
}

void WorkMonitor::initialize() {
  stopped.store(false);
  WORK_MONITOR.start();
}

void WorkMonitor::shutdown() {
  stopped.store(true);
  WORK_MONITOR.beginShutdown();
}

AqlWorkStack::AqlWorkStack(TRI_vocbase_t* vocbase, uint64_t queryId,
                           char const* text, size_t length) {
  WorkMonitor::pushAql(vocbase, queryId, text, length);
}

AqlWorkStack::AqlWorkStack(TRI_vocbase_t* vocbase, uint64_t queryId) {
  WorkMonitor::pushAql(vocbase, queryId);
}

AqlWorkStack::~AqlWorkStack() { WorkMonitor::popAql(); }

CustomWorkStack::CustomWorkStack(char const* type, char const* text,
                                 size_t length) {
  WorkMonitor::pushCustom(type, text, length);
}

CustomWorkStack::CustomWorkStack(char const* type, uint64_t id) {
  WorkMonitor::pushCustom(type, id);
}

CustomWorkStack::~CustomWorkStack() { WorkMonitor::popCustom(); }
