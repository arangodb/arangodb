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

#include "VelocysCommTask.h"

#include <openssl/err.h>

#include "Basics/logging.h"
#include "Basics/socket-utils.h"
#include "Basics/ssl-helper.h"
#include "Basics/StringBuffer.h"
#include "VelocyServer/VelocysServer.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb::rest;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task with a given socket
////////////////////////////////////////////////////////////////////////////////

VelocysCommTask::VelocysCommTask(VelocysServer* server, TRI_socket_t socket,
                             ConnectionInfo const& info,
                             double keepAliveTimeout, SSL_CTX* ctx,
                             int verificationMode,
                             GeneralRequest::ProtocolVersion version, GeneralRequest::RequestType request,
                             int (*verificationCallback)(int, X509_STORE_CTX*))
    : Task("VelocysCommTask"),
    VelocyCommTask(server, socket, info, keepAliveTimeout, "VelocysCommTask", GeneralRequest::VSTREAM_UNKNOWN, 
                    GeneralRequest::VSTREAM_REQUEST_ILLEGAL),
    GeneralsCommTask(server, socket, info, keepAliveTimeout, ctx, verificationMode, 
                      "VelocysCommTask", version, request, (*verificationCallback)(int, X509_STORE_CTX*))
    {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

VelocysCommTask::~VelocysCommTask() {
  shutdownSsl(true);

  delete[] _tmpReadBuffer;
}



bool VelocysCommTask::setup(Scheduler* scheduler, EventLoop loop) {
  // setup base class
  bool ok = VelocyCommTask::setup(scheduler, loop);

  if (!ok) {
    return false;
  }

  // build a new connection
  TRI_ASSERT(_ssl == nullptr);

  ERR_clear_error();
  _ssl = SSL_new(_ctx);

  _connectionInfo.sslContext = _ssl;

  if (_ssl == nullptr) {
    LOG_DEBUG("cannot build new SSL connection: %s",
              arangodb::basics::lastSSLError().c_str());

    shutdownSsl(false);
    return false;  // terminate ourselves, ssl is nullptr
  }

  // enforce verification
  ERR_clear_error();
  SSL_set_verify(_ssl, _verificationMode, _verificationCallback);

  // with the file descriptor
  ERR_clear_error();
  SSL_set_fd(_ssl, (int)TRI_get_fd_or_handle_of_socket(_commSocket));

  // accept might need writes
  _scheduler->startSocketEvents(_writeWatcher);

  return true;
}


bool VelocysCommTask::handleEvent(EventToken token, EventType revents) {
  // try to accept the SSL connection
  if (!_accepted) {
    bool result = false;  // be pessimistic

    if ((token == _readWatcher && (revents & EVENT_SOCKET_READ)) ||
        (token == _writeWatcher && (revents & EVENT_SOCKET_WRITE))) {
      // must do the SSL handshake first
      result = trySSLAccept();
    }

    if (!result) {
      // status is somehow invalid. we got here even though no accept was ever
      // successful
      _clientClosed = true;
      // this will remove the corresponding chunkedTask from the global list
      // if we would leave it in there, then the server may crash on shutdown
      _server->handleCommunicationFailure(this);

      _scheduler->destroyTask(this);
    }

    return result;
  }

  // if we blocked on write, read can be called when the socket is writeable
  if (_readBlockedOnWrite && token == _writeWatcher &&
      (revents & EVENT_SOCKET_WRITE)) {
    _readBlockedOnWrite = false;
    revents &= ~EVENT_SOCKET_WRITE;
    revents |= EVENT_SOCKET_READ;
    token = _readWatcher;
  }

  // handle normal socket operation
  bool result = VelocyCommTask::handleEvent(token, revents);

  // warning: if _clientClosed is true here, the task (this) is already deleted!

  // we might need to start listing for writes (even we only want to READ)
  if (result && !_clientClosed) {
    if (_readBlockedOnWrite || _writeBlockedOnRead) {
      _scheduler->startSocketEvents(_writeWatcher);
    }
  }

  return result;
}