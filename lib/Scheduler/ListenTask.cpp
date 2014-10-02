////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to establish connections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ListenTask.h"

#include <errno.h>


#ifdef TRI_HAVE_LINUX_SOCKETS
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif


#ifdef TRI_HAVE_WINSOCK2_H
#include "Basics/win-utils.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#include <sys/types.h>

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/logging.h"
#include "Basics/socket-utils.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

ListenTask::ListenTask (Endpoint* endpoint)
  : Task("ListenTask"),
    readWatcher(0),
    _endpoint(endpoint),
    acceptFailures(0) {
  TRI_invalidatesocket(&_listenSocket);
  bindSocket();
}


ListenTask::~ListenTask () {
  if (readWatcher != 0) {
    _scheduler->uninstallEvent(readWatcher);
  }
}

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

bool ListenTask::isBound () const {
  MUTEX_LOCKER(changeLock);

  return _endpoint != 0 && _endpoint->isConnected();
}



// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------


bool ListenTask::setup (Scheduler* scheduler, EventLoop loop) {
  if (! isBound()) {
    return true;
  }


#ifdef _WIN32

  // ..........................................................................
  // The problem we have here is that this opening of the fs handle may fail.
  // There is no mechanism to the calling function to report failure.
  // ..........................................................................

  LOG_TRACE("attempting to convert socket handle to socket descriptor");

  if (! TRI_isvalidsocket(_listenSocket)) {
    LOG_ERROR("In ListenTask::setup could not convert socket handle to socket descriptor -- invalid socket handle");
    return false;
  }

  // For the official version of libev we would do this:
  // int res = _open_osfhandle (_listenSocket.fileHandle, 0);
  // However, this opens a whole lot of problems and in general one should
  // never use _open_osfhandle for sockets.
  // Therefore, we do the following, although it has the potential to
  // lose the higher bits of the socket handle:
  int res = (int)_listenSocket.fileHandle;

  if (res == - 1) {
    LOG_ERROR("In ListenTask::setup could not convert socket handle to socket descriptor -- _open_osfhandle(...) failed");

    res = TRI_CLOSE_SOCKET(_listenSocket);

    if (res != 0) {
      res = WSAGetLastError();
      LOG_ERROR("In ListenTask::setup closesocket(...) failed with error code: %d", (int) res);
    }
    TRI_invalidatesocket(&_listenSocket);
    return false;
  }

  _listenSocket.fileDescriptor = res;

#endif

  this->_scheduler = scheduler;
  this->_loop = loop;
  readWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this, _listenSocket);

  if (readWatcher == -1) {
    return false;
  }
  return true;
}



void ListenTask::cleanup () {
  if (_scheduler == 0) {
    LOG_WARNING("In ListenTask::cleanup the scheduler has disappeared -- invalid pointer");
    readWatcher = 0;
    return;
  }
  _scheduler->uninstallEvent(readWatcher);
  readWatcher = 0;
}



bool ListenTask::handleEvent (EventToken token, EventType revents) {
  if (token == readWatcher) {
    if ((revents & EVENT_SOCKET_READ) == 0) {
      return true;
    }

    sockaddr_in addr;
    socklen_t len = sizeof(addr);

    memset(&addr, 0, sizeof(addr));

    // accept connection
    TRI_socket_t connectionSocket;
    connectionSocket = TRI_accept(_listenSocket, (sockaddr*) &addr, &len);

    if (! TRI_isvalidsocket(connectionSocket)) {
      ++acceptFailures;

      if (acceptFailures < MAX_ACCEPT_ERRORS) {
        LOG_WARNING("accept failed with %d (%s)", (int) errno, strerror(errno));
      }
      else if (acceptFailures == MAX_ACCEPT_ERRORS) {
        LOG_ERROR("too many accept failures, stopping logging");
      }

      // TODO GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

      return true;
    }

    acceptFailures = 0;

    struct sockaddr_in addr_out;
    socklen_t len_out = sizeof(addr_out);

    int res = TRI_getsockname(connectionSocket, (sockaddr*) &addr_out, &len_out);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_CLOSE_SOCKET(connectionSocket);

      LOG_WARNING("getsockname failed with %d (%s)", errno, strerror(errno));

      // TODO GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

      return true;
    }

    // disable nagle's algorithm, set to non-blocking and close-on-exec
    bool result = _endpoint->initIncoming(connectionSocket);

    if (! result) {
      TRI_CLOSE_SOCKET(connectionSocket);

      // TODO GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

      return true;
    }

    // set client address and port
    ConnectionInfo info;

    char host[NI_MAXHOST], serv[NI_MAXSERV];

    if (getnameinfo((sockaddr*) &addr, len,
                    host, sizeof(host),
                    serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {

      info.clientAddress = string(host);
      info.clientPort = addr.sin_port;
    }
    else {
      info.clientAddress = inet_ntoa(addr.sin_addr);
      info.clientPort = addr.sin_port;
    }

    info.serverAddress = _endpoint->getHost();
    info.serverPort    = _endpoint->getPort();
    info.endpoint      = _endpoint->getSpecification();
    info.endpointType  = _endpoint->getDomainType();

    return handleConnected(connectionSocket, info);
  }

  return true;
}

// -----------------------------------------------------------------------------
// private methods
// -----------------------------------------------------------------------------

bool ListenTask::bindSocket () {
  _listenSocket = _endpoint->connect(30, 300); // connect timeout in seconds

  if (! TRI_isvalidsocket(_listenSocket)) {
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
