////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_HTTP_SERVER_REST_ENGINE_H
#define ARANGOD_HTTP_SERVER_REST_ENGINE_H 1

#include "Basics/Common.h"

#include "GeneralServer/RestStatus.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb {
class RestEngine;

namespace rest {
class RestHandler;
}

class RestEngine {
 public:
  enum class State { PREPARE, EXECUTE, RUN, FINALIZE, WAITING, DONE, FAILED };

 public:
  RestEngine() {}

 public:
  void init(EventLoop loop) { _loop = loop; }

  int asyncRun(std::shared_ptr<rest::RestHandler>);
  int syncRun(std::shared_ptr<rest::RestHandler>);

  void setState(State state) { _state = state; }
  void appendRestStatus(std::shared_ptr<RestStatusElement>);

  void queue(std::function<void()> callback) {
    _loop._scheduler->post(callback);
  }

  bool hasSteps() { return !_elements.empty(); }

  std::shared_ptr<RestStatusElement> popStep() {
    auto element = *_elements.rbegin();
    _elements.pop_back();
    return element;
  }

 private:
  int run(std::shared_ptr<rest::RestHandler>, bool synchronous);

 private:
  State _state = State::PREPARE;
  std::vector<std::shared_ptr<RestStatusElement>> _elements;

  EventLoop _loop;
};
}  // namespace arangodb

#endif
