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

#ifndef ARANGOD_SCHEDULER_LISTEN_TASK_H
#define ARANGOD_SCHEDULER_LISTEN_TASK_H 1

#include "Scheduler/Task.h"

#include "Basics/Mutex.h"
#include "Endpoint/ConnectionInfo.h"
#include "Endpoint/Endpoint.h"
#include "Scheduler/Acceptor.h"
#include "Scheduler/Socket.h"

namespace arangodb {
class ListenTask : virtual public rest::Task {
 public:
  static size_t const MAX_ACCEPT_ERRORS = 128;

 public:
  ListenTask(rest::Scheduler*, Endpoint*);
  ~ListenTask();

 public:
  virtual void handleConnected(std::unique_ptr<Socket>, ConnectionInfo&&) = 0;

 public:
  Endpoint* endpoint() const { return _endpoint; }

  bool start();
  void stop();

 private:
  Endpoint* _endpoint;
  size_t _acceptFailures = 0;

  Mutex _shutdownMutex;
  bool _bound;

  std::unique_ptr<Acceptor> _acceptor;
  std::function<void(asio_ns::error_code const&)> _handler;
};
}

#endif
