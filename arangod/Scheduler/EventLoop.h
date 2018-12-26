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

#ifndef ARANGOD_SCHEDULER_EVENTS_H
#define ARANGOD_SCHEDULER_EVENTS_H 1

#include "Basics/Common.h"

#include "Basics/asio-helper.h"

namespace arangodb {
namespace rest {
class Scheduler;
}

struct EventLoop {
  EventLoop(boost::asio::io_service* ioService, rest::Scheduler* scheduler)
      : _ioService(ioService), _scheduler(scheduler) {}

  EventLoop() : EventLoop(nullptr, nullptr) {}

  boost::asio::io_service* _ioService;
  rest::Scheduler* _scheduler;
};
}  // namespace arangodb

#endif
