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

#ifndef ARANGOD_SCHEDULER_TIMER_TASK_H
#define ARANGOD_SCHEDULER_TIMER_TASK_H 1

#include "Basics/Common.h"
#include "Scheduler/Task.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Scheduler
/// @brief task used to handle timer events
////////////////////////////////////////////////////////////////////////////////

class TimerTask : virtual public Task {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new task for a timer event
  //////////////////////////////////////////////////////////////////////////////

  TimerTask(std::string const&, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a task specific description in VelocyPack format
  //////////////////////////////////////////////////////////////////////////////

  virtual void getDescription(arangodb::velocypack::Builder&) const override;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief called when the timer is reached
  //////////////////////////////////////////////////////////////////////////////

  virtual bool handleTimeout() = 0;

 protected:
  ~TimerTask();

 protected:
  bool setup(Scheduler*, EventLoop) override;

  void cleanup() override;

  bool handleEvent(EventToken token, EventType event) override;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief timer event
  //////////////////////////////////////////////////////////////////////////////

  EventToken _watcher;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief timeout
  //////////////////////////////////////////////////////////////////////////////

  double _seconds;
};
}
}

#endif
