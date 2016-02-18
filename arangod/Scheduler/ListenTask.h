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

#include "Basics/Common.h"

#ifdef TRI_HAVE_LINUX_SOCKETS
#include <netdb.h>
#endif

#ifdef TRI_HAVE_WINSOCK2_H
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "Scheduler/Task.h"

#include "Basics/Mutex.h"
#include "Basics/socket-utils.h"
#include "Rest/ConnectionInfo.h"
#include "Rest/Endpoint.h"

namespace arangodb {
namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Scheduler
/// @brief task used to establish connections
////////////////////////////////////////////////////////////////////////////////

class ListenTask : virtual public Task {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximal number of failed connects
  //////////////////////////////////////////////////////////////////////////////

  static size_t const MAX_ACCEPT_ERRORS = 128;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief listen to given endpoint
  //////////////////////////////////////////////////////////////////////////////

  explicit ListenTask(Endpoint*);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if listen socket is bound
  //////////////////////////////////////////////////////////////////////////////

  bool isBound() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the endpoint
  //////////////////////////////////////////////////////////////////////////////

  Endpoint* endpoint() const { return _endpoint; }

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief called by the task to indicate connection success
  //////////////////////////////////////////////////////////////////////////////

  virtual bool handleConnected(TRI_socket_t socket,
                               ConnectionInfo const& info, bool isHttp) = 0;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief destructs a listen task
  ///
  /// This method will close the underlying socket.
  //////////////////////////////////////////////////////////////////////////////

  ~ListenTask();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// {@inheritDoc}
  ///
  /// Note that registerTask must only be called when the socket is bound.
  //////////////////////////////////////////////////////////////////////////////

  bool setup(Scheduler*, EventLoop) override;

  void cleanup() override;

  bool handleEvent(EventToken token, EventType) override;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief event for read
  //////////////////////////////////////////////////////////////////////////////

  EventToken _readWatcher;

 private:
  bool bindSocket();

 private:
  Endpoint* _endpoint;

  TRI_socket_t _listenSocket;

  size_t _acceptFailures;

  mutable Mutex _changeLock;
};
}
}

#endif
