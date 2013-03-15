////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to handle signals
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SignalTask.h"

#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"

#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

SignalTask::SignalTask ()
  : Task("SignalTask") {
  for (size_t i = 0;  i < MAX_SIGNALS;  ++i) {
    watcher[i] = 0;
  }
}



SignalTask::~SignalTask () {
}

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

bool SignalTask::addSignal (int signal) {
  MUTEX_LOCKER(changeLock);

  if (signals.size() >= MAX_SIGNALS) {
    LOGGER_ERROR("maximal number of signals reached");
    return false;
  }
  else {
    if (scheduler != 0) {
      scheduler->unregisterTask(this);
    }

    signals.insert(signal);

    if (scheduler != 0) {
      scheduler->registerTask(this);
    }

    return true;
  }
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

bool SignalTask::setup (Scheduler* scheduler, EventLoop loop) {
  this->scheduler = scheduler;
  this->loop = loop;

  size_t pos = 0;

  for (set<int>::iterator i = signals.begin();  i != signals.end() && pos < MAX_SIGNALS;  ++i, ++pos) {
    watcher[pos] = scheduler->installSignalEvent(loop, this, *i);
  }
  return true;
}



void SignalTask::cleanup () {
  if (scheduler == 0) {
    LOGGER_WARNING("In SignalTask::cleanup the scheduler has disappeared -- invalid pointer");
  }
  for (size_t pos = 0;  pos < signals.size() && pos < MAX_SIGNALS;  ++pos) {
    if (scheduler != 0) {
      scheduler->uninstallEvent(watcher[pos]);
    }
    watcher[pos] = 0;
  }
}



bool SignalTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;

  if (revents & EVENT_SIGNAL) {
    for (size_t pos = 0;  pos < signals.size() && pos < MAX_SIGNALS;  ++pos) {
      if (token == watcher[pos]) {
        result = handleSignal();
        break;
      }
    }
  }

  return result;
}



bool SignalTask::needsMainEventLoop () const {
  return true;
}

