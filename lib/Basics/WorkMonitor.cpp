////////////////////////////////////////////////////////////////////////////////
/// @brief work monitor class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "WorkMonitor.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/tri-strings.h"
#include "velocypack/Builder.h"
#include "velocypack/Dumper.h"
#include "velocypack/Sink.h"
#include "velocypack/velocypack-aliases.h"

#include <boost/lockfree/queue.hpp>

using namespace arangodb;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                             class CustomWorkStack
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

CustomWorkStack::CustomWorkStack (const char* type, const char* text, size_t length) {
  WorkMonitor::pushCustom(type, text, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

CustomWorkStack::~CustomWorkStack () {
  WorkMonitor::popCustom();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief singleton
////////////////////////////////////////////////////////////////////////////////

static WorkMonitor WORK_MONITOR;

////////////////////////////////////////////////////////////////////////////////
/// @brief current work item
////////////////////////////////////////////////////////////////////////////////

static thread_local Thread* CURRENT_THREAD = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief current work item
////////////////////////////////////////////////////////////////////////////////

static thread_local WorkDescription* CURRENT_WORK_DESCRIPTION = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief all known threads
////////////////////////////////////////////////////////////////////////////////

static std::set<Thread*> THREADS;

////////////////////////////////////////////////////////////////////////////////
/// @brief guard for THREADS
////////////////////////////////////////////////////////////////////////////////

static Mutex THREADS_LOCK;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of free descriptions
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<WorkDescription*> EMPTY_WORK_DESCRIPTION(128);

////////////////////////////////////////////////////////////////////////////////
/// @brief list of freeable descriptions
////////////////////////////////////////////////////////////////////////////////

static boost::lockfree::queue<WorkDescription*> FREEABLE_WORK_DESCRIPTION(128);


////////////////////////////////////////////////////////////////////////////////
/// @brief stopped flag
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> WORK_MONITOR_STOPPED(false);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a description and its resources
////////////////////////////////////////////////////////////////////////////////

static void deleteWorkDescription (WorkDescription* desc) {
  if (desc->_destroy) {
    switch (desc->_type) {
      case WorkType::THREAD:
      case WorkType::CUSTOM:
        break;

      case WorkType::HANDLER:
        WorkMonitor::DELETE_HANDLER(desc);
        break;
    }
  }

  EMPTY_WORK_DESCRIPTION.push(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief vpack representation of a work description
////////////////////////////////////////////////////////////////////////////////

#undef SHOW_RESULTS
// TODO(fc) use vpackWorkDescription
#ifdef SHOW_RESULTS
#include <iostream>

static void vpackWorkDescription (VPackBuilder* b, WorkDescription* desc) {
  switch (desc->_type) {
    case WorkType::THREAD:
      b->add("type", VPackValue("thread"));
      b->add("name", VPackValue(desc->_data.thread->name()));
      b->add("status", VPackValue(VPackValueType::Object));
      desc->_data.thread->addStatus(b);
      b->close();
      break;

    case WorkType::CUSTOM:
      b->add("type", VPackValue(desc->_customType));
      b->add("description", VPackValue(desc->_data.text));
      break;

    case WorkType::HANDLER:
      WorkMonitor::VPACK_HANDLER(b, desc);
      break;
  }

  if (desc->_prev != nullptr) {
    b->add("parent", VPackValue(VPackValueType::Object));
    vpackWorkDescription(b, desc->_prev);
    b->close();
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                            struct WorkDescription
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

WorkDescription::WorkDescription (WorkType type, WorkDescription* prev)
  : _type(type), _destroy(true), _prev(prev) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class WorkMonitor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors a new monitor
////////////////////////////////////////////////////////////////////////////////

WorkMonitor::WorkMonitor ()
  : Thread("Work Monitor"), _stopping(false) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty WorkDescription
////////////////////////////////////////////////////////////////////////////////

WorkDescription* WorkMonitor::createWorkDescription (WorkType type) {
  WorkDescription* desc = nullptr;
  WorkDescription* prev = (CURRENT_THREAD == nullptr)
    ? CURRENT_WORK_DESCRIPTION
    : CURRENT_THREAD->workDescription();

  if (EMPTY_WORK_DESCRIPTION.pop(desc) && desc != nullptr) {
    desc->_type = type;
    desc->_prev = prev;
    desc->_destroy = true;
  }
  else {
    desc = new WorkDescription(type, prev);
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief activates a WorkDescription
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::activateWorkDescription (WorkDescription* desc) {
  if (CURRENT_THREAD == nullptr) {
    CURRENT_WORK_DESCRIPTION = desc;
  }
  else {
    CURRENT_THREAD->setWorkDescription(desc);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivates a WorkDescription
////////////////////////////////////////////////////////////////////////////////

WorkDescription* WorkMonitor::deactivateWorkDescription () {
  if (CURRENT_THREAD == nullptr) {
    WorkDescription* desc = CURRENT_WORK_DESCRIPTION;
    CURRENT_WORK_DESCRIPTION = desc->_prev;
    return desc;
  }
  else {
    return CURRENT_THREAD->setPrevWorkDescription();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees an WorkDescription
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::freeWorkDescription (WorkDescription* desc) {
  if (WORK_MONITOR_STOPPED) {
    deleteWorkDescription(desc);
  }
  else {
    FREEABLE_WORK_DESCRIPTION.push(desc);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes a thread
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::pushThread (Thread* thread) {
  CURRENT_THREAD = thread;

  WorkDescription* desc = createWorkDescription(WorkType::THREAD);
  desc->_data.thread = thread;

  activateWorkDescription(desc);

  {
    MutexLocker guard(&THREADS_LOCK);
    THREADS.insert(thread);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops a thread
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::popThread (Thread* thread) {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc->_type == WorkType::THREAD);
  TRI_ASSERT(desc->_data.thread == thread);

  freeWorkDescription(desc);

  {
    MutexLocker guard(&THREADS_LOCK);
    THREADS.erase(thread);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes a custom task
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::pushCustom (const char* type, const char* text, size_t length) {
  WorkDescription* desc = createWorkDescription(WorkType::CUSTOM);

  if (sizeof(desc->_data.text) < length) {
    length = sizeof(desc->_data.text);
  }

  TRI_CopyString(desc->_customType, type, sizeof(desc->_customType));
  TRI_CopyString(desc->_data.text, text, length);

  activateWorkDescription(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops a custom task
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::popCustom () {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc->_type == WorkType::CUSTOM);

  freeWorkDescription(desc);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::run () {
  const uint32_t maxSleep = 100 * 1000;
  const uint32_t minSleep = 100;
  uint32_t s = minSleep;

#ifdef SHOW_RESULTS
  double x = 0;
#endif

  // clean old entries and create summary if requested
  while (! _stopping) {
    bool found = false;
    WorkDescription* desc;

    while (FREEABLE_WORK_DESCRIPTION.pop(desc)) {
      found = true;
      deleteWorkDescription(desc);
    }

    if (found) {
      s = minSleep;
    }
    else if (s < maxSleep) {
      s *= 2;
    }

// TODO(fc) trigger output
#ifdef SHOW_RESULTS
    double y = TRI_microtime();

    if (x + 10 < y) {
      x = y;

      MutexLocker guard(&THREADS_LOCK);
      VPackBuilder b;
      b.add(VPackValue(VPackValueType::Array));

      for (auto& thread : THREADS) {
        WorkDescription* desc = thread->workDescription();

        if (desc != nullptr) {
          b.add(VPackValue(VPackValueType::Object));
          vpackWorkDescription(&b, desc);
          b.close();
        }
      }

      b.close();

      VPackSlice s(b.start());

      VPackOptions options;
      options.prettyPrint = true;

      std::string buffer;
      VPackStringSink sink(&buffer);

      VPackDumper dumper(&sink, &options);
      dumper.dump(s);

      std::cout << buffer << "\n";
    }
#endif

    usleep(s);
  }

  // indicate that we stopped the work monitor, freeWorkDescription
  // should directly delete old entries
  WORK_MONITOR_STOPPED = true;

  // cleanup old entries
  WorkDescription* desc;

  while (FREEABLE_WORK_DESCRIPTION.pop(desc) && desc != nullptr) {
    deleteWorkDescription(desc);
  }
  
  while (EMPTY_WORK_DESCRIPTION.pop(desc) && desc != nullptr) {
    delete desc;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    module methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the work monitor
////////////////////////////////////////////////////////////////////////////////

void arangodb::InitializeWorkMonitor () {
  WORK_MONITOR.start();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the work monitor
////////////////////////////////////////////////////////////////////////////////

void arangodb::ShutdownWorkMonitor () {
  WORK_MONITOR.shutdown();
  WORK_MONITOR.join();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
