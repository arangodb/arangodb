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

#include "PeriodicTask.h"
#include "Scheduler/Scheduler.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::rest;

PeriodicTask::PeriodicTask(std::string const& id, double offset,
                           double interval)
    : Task(id, "PeriodicTask"),
      _watcher(nullptr),
      _offset(offset),
      _interval(interval) {}

PeriodicTask::~PeriodicTask() { cleanup(); }

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

void PeriodicTask::resetTimer(double offset, double interval) {
  _scheduler->rearmPeriodic(_watcher, offset, interval);
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in VelocyPack format
////////////////////////////////////////////////////////////////////////////////

void PeriodicTask::getDescription(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("type", VPackValue("periodic"));
  builder.add("period", VPackValue(_interval));
}

bool PeriodicTask::setup(Scheduler* scheduler, EventLoop loop) {
  this->_scheduler = scheduler;
  this->_loop = loop;

  _watcher = scheduler->installPeriodicEvent(loop, this, _offset, _interval);

  return true;
}

void PeriodicTask::cleanup() {
  if (_scheduler != nullptr) {
    _scheduler->uninstallEvent(_watcher);
  }
  _watcher = nullptr;
}

bool PeriodicTask::handleEvent(EventToken token, EventType revents) {
  bool result = true;

  if (token == _watcher && (revents & EVENT_PERIODIC)) {
    result = handlePeriod();
  }

  return result;
}
