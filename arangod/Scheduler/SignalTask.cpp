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

#include "SignalTask.h"

#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"

#include "Scheduler/Scheduler.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

SignalTask::SignalTask() : Task("SignalTask") {
  for (size_t i = 0; i < MAX_SIGNALS; ++i) {
    _watchers[i] = nullptr;
  }
}

SignalTask::~SignalTask() { cleanup(); }

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

bool SignalTask::addSignal(int signal) {
  MUTEX_LOCKER(mutexLocker, _changeLock);

  if (_signals.size() >= MAX_SIGNALS) {
    LOG(ERR) << "maximal number of signals reached";
    return false;
  } else {
    if (_scheduler != nullptr) {
      _scheduler->unregisterTask(this);
    }

    _signals.insert(signal);

    if (_scheduler != nullptr) {
      _scheduler->registerTask(this);
    }

    return true;
  }
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

bool SignalTask::setup(Scheduler* scheduler, EventLoop loop) {
  _scheduler = scheduler;
  _loop = loop;

  size_t pos = 0;

  for (auto& it : _signals) {
    _watchers[pos++] = _scheduler->installSignalEvent(loop, this, it);
  }

  return true;
}

void SignalTask::cleanup() {
  for (size_t pos = 0; pos < _signals.size() && pos < MAX_SIGNALS; ++pos) {
    if (_scheduler != nullptr) {
      _scheduler->uninstallEvent(_watchers[pos]);
    }
    _watchers[pos] = nullptr;
  }
}

bool SignalTask::handleEvent(EventToken token, EventType revents) {
  TRI_ASSERT(token != nullptr);

  bool result = true;

  if (revents & EVENT_SIGNAL) {
    for (size_t pos = 0; pos < _signals.size() && pos < MAX_SIGNALS; ++pos) {
      if (token == _watchers[pos]) {
        result = handleSignal();
        break;
      }
    }
  }

  return result;
}

bool SignalTask::needsMainEventLoop() const { return true; }
