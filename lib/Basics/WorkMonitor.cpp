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

#include "Basics/MutexLocker.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
std::atomic<uint64_t> NEXT_DESC_ID(static_cast<uint64_t>(0));
}

// -----------------------------------------------------------------------------
// --SECTION--                                            private static members
// -----------------------------------------------------------------------------

std::atomic<bool> WorkMonitor::_stopped(true);

boost::lockfree::queue<WorkDescription*> WorkMonitor::_emptyWorkDescription(128);

boost::lockfree::queue<WorkDescription*> WorkMonitor::_freeableWorkDescription(128);

boost::lockfree::queue<std::pair<std::shared_ptr<rest::RestHandler>, std::function<void()>>*>
    WorkMonitor::_workOverview(128);

Mutex WorkMonitor::_cancelLock;
std::set<uint64_t> WorkMonitor::_cancelIds;

Mutex WorkMonitor::_threadsLock;
std::set<Thread*> WorkMonitor::_threads;

static WorkMonitor WORK_MONITOR;
static thread_local WorkDescription* CURRENT_WORK_DESCRIPTION = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

void WorkMonitor::freeWorkDescription(WorkDescription* desc) {
  if (_stopped.load()) {
    deleteWorkDescription(desc, true);
  } else {
    desc->_context.reset();
    _freeableWorkDescription.push(desc);
  }
}

bool WorkMonitor::pushThread(Thread* thread) {
  if (_stopped.load()) {
    return false;
  }

  TRI_ASSERT(thread != nullptr);
  TRI_ASSERT(Thread::CURRENT_THREAD == nullptr);
  Thread::CURRENT_THREAD = thread;

  try {
    WorkDescription* desc = createWorkDescription(WorkType::THREAD);

    new (&desc->_data._thread) WorkDescription::Data::ThreadMember(thread, false);

    activateWorkDescription(desc);

    {
      MUTEX_LOCKER(guard, _threadsLock);
      _threads.insert(thread);
    }
  } catch (...) {
    throw;
  }

  return true;
}

void WorkMonitor::popThread(Thread* thread) {
  TRI_ASSERT(thread != nullptr);
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc->_type == WorkType::THREAD);
  TRI_ASSERT(desc->_data._thread._thread == thread);

  try {
    freeWorkDescription(desc);

    {
      MUTEX_LOCKER(guard, _threadsLock);
      _threads.erase(thread);
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

  desc->_data._aql._vocbase = vocbase;
  desc->_data._aql._id = queryId;

  if (sizeof(desc->_data._aql._text) - 1 < length) {
    length = sizeof(desc->_data._aql._text) - 1;
  }

  TRI_CopyString(desc->_data._aql._text, text, length);
  new (&desc->_data._aql._canceled) std::atomic<bool>(false);

  activateWorkDescription(desc);
}

void WorkMonitor::pushAql(TRI_vocbase_t* vocbase, uint64_t queryId) {
  TRI_ASSERT(vocbase != nullptr);

  WorkDescription* desc = createWorkDescription(WorkType::AQL_ID);
  TRI_ASSERT(desc != nullptr);

  desc->_data._aql._vocbase = vocbase;
  desc->_data._aql._id = queryId;

  *(desc->_data._aql._text) = '\0';
  new (&desc->_data._aql._canceled) std::atomic<bool>(false);

  activateWorkDescription(desc);
}

void WorkMonitor::popAql() {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc != nullptr);
  TRI_ASSERT(desc->_type == WorkType::AQL_STRING || desc->_type == WorkType::AQL_ID);

  try {
    freeWorkDescription(desc);
  } catch (...) {
    // just to prevent throwing exceptions from here, as this method
    // will be called in destructors...
  }
}

void WorkMonitor::pushCustom(char const* type, char const* text, size_t length) {
  TRI_ASSERT(type != nullptr);
  TRI_ASSERT(text != nullptr);

  WorkDescription* desc = createWorkDescription(WorkType::CUSTOM);
  TRI_ASSERT(desc != nullptr);

  TRI_CopyString(desc->_data._custom._type, type, sizeof(desc->_data._custom._type) - 1);

  if (sizeof(desc->_data._custom._text) - 1 < length) {
    length = sizeof(desc->_data._custom._text) - 1;
  }

  TRI_CopyString(desc->_data._custom._text, text, length);

  activateWorkDescription(desc);
}

void WorkMonitor::pushCustom(char const* type, uint64_t id) {
  TRI_ASSERT(type != nullptr);

  WorkDescription* desc = createWorkDescription(WorkType::CUSTOM);
  TRI_ASSERT(desc != nullptr);

  TRI_CopyString(desc->_data._custom._type, type, sizeof(desc->_data._custom._type) - 1);

  std::string idString = std::to_string(id);
  TRI_CopyString(desc->_data._custom._text, idString.c_str(), idString.size());

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

void WorkMonitor::requestWorkOverview(std::shared_ptr<rest::RestHandler> handler,
                                      std::function<void()> next) {
  _workOverview.push(
      new std::pair<std::shared_ptr<rest::RestHandler>, std::function<void()>>(handler, next));
}

void WorkMonitor::cancelWork(uint64_t id) {
  MUTEX_LOCKER(guard, _cancelLock);
  _cancelIds.insert(id);
}

void WorkMonitor::initialize() {
  _stopped.store(false);
  WORK_MONITOR.start();
}

void WorkMonitor::shutdown() {
  _stopped.store(true);
  WORK_MONITOR.beginShutdown();
}

void WorkMonitor::clearHandlers() {
  if (_stopped.load()) {
    return;
  }

  WORK_MONITOR.clearAllHandlers();
}

// -----------------------------------------------------------------------------
// --SECTION--                                            static private methods
// -----------------------------------------------------------------------------

WorkDescription* WorkMonitor::createWorkDescription(WorkType type) {
  WorkDescription* desc = nullptr;
  WorkDescription* prev = (Thread::CURRENT_THREAD == nullptr)
                              ? CURRENT_WORK_DESCRIPTION
                              : Thread::CURRENT_THREAD->workDescription();

  if (_emptyWorkDescription.pop(desc) && desc != nullptr) {
    desc->_type = type;
    desc->_prev.store(prev);
  } else {
    desc = new WorkDescription(type, prev);
  }

  desc->_id = NEXT_DESC_ID.fetch_add(1, std::memory_order_seq_cst);

  return desc;
}

void WorkMonitor::deleteWorkDescription(WorkDescription* desc, bool stopped) {
  desc->_context.reset();

  switch (desc->_type) {
    case WorkType::THREAD:
      desc->_data._thread._canceled.std::atomic<bool>::~atomic();
      break;

    case WorkType::HANDLER:
      deleteHandler(desc);
      break;

    case WorkType::AQL_ID:
    case WorkType::AQL_STRING:
      desc->_data._aql._canceled.std::atomic<bool>::~atomic();
      break;

    case WorkType::CUSTOM:
      break;
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
  _emptyWorkDescription.push(desc);
}

void WorkMonitor::activateWorkDescription(WorkDescription* desc) {
  if (Thread::CURRENT_THREAD == nullptr) {
    TRI_ASSERT(CURRENT_WORK_DESCRIPTION == nullptr);
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
      b->add("name", VPackValue(desc->_data._thread._thread->name()));
      b->add("number", VPackValue(desc->_data._thread._thread->threadNumber()));
      b->add("status", VPackValue(VPackValueType::Object));
      desc->_data._thread._thread->addStatus(b);
      b->close();
      break;

    case WorkType::CUSTOM:
      b->add("type", VPackValue(desc->_data._custom._type));
      b->add("description", VPackValue(desc->_data._custom._text));
      break;

    case WorkType::AQL_STRING:
      b->add("type", VPackValue("AQL query"));
      b->add("queryId", VPackValue(desc->_data._aql._id));
      b->add("description", VPackValue(desc->_data._aql._text));
      break;

    case WorkType::AQL_ID:
      b->add("type", VPackValue("AQL query id"));
      b->add("queryId", VPackValue(desc->_data._aql._id));
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

    if (_cancelIds.find(id) != _cancelIds.end()) {
      for (auto it = path.rbegin(); it < path.rend(); ++it) {
        bool descent = true;
        WorkDescription* d = *it;

        switch (d->_type) {
          case WorkType::THREAD:
            d->_data._thread._canceled.store(true);
            break;

          case WorkType::HANDLER:
            d->_data._handler._canceled.store(true);
            break;

          case WorkType::AQL_STRING:
          case WorkType::AQL_ID:
            descent = cancelAql(d);
            break;

          case WorkType::CUSTOM:
            d->_data._thread._canceled.store(true);
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
