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

#include "ListenTask.h"

#include <errno.h>

#ifdef TRI_HAVE_WINSOCK2_H
#include "Basics/win-utils.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "Basics/socket-utils.h"
#include "Basics/StringUtils.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

ListenTask::ListenTask(Endpoint* endpoint)
    : Task("ListenTask"),
      _readWatcher(nullptr),
      _endpoint(endpoint),
      _acceptFailures(0) {
  TRI_invalidatesocket(&_listenSocket);
  bindSocket();
}

ListenTask::~ListenTask() {
  if (_readWatcher != nullptr) {
    _scheduler->uninstallEvent(_readWatcher);
  }
}

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

bool ListenTask::isBound() const {
  MUTEX_LOCKER(mutexLocker, _changeLock);  // FIX_MUTEX ?

  return _endpoint != nullptr && _endpoint->isConnected();
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

bool ListenTask::setup(Scheduler* scheduler, EventLoop loop) {
  if (!isBound()) {
    return true;
  }

#ifdef _WIN32

  // ..........................................................................
  // The problem we have here is that this opening of the fs handle may fail.
  // There is no mechanism to the calling function to report failure.
  // ..........................................................................

  LOG(TRACE) << "attempting to convert socket handle to socket descriptor";

  if (!TRI_isvalidsocket(_listenSocket)) {
    LOG(ERR) << "In ListenTask::setup could not convert socket handle to socket descriptor -- invalid socket handle";
    return false;
  }

  // For the official version of libev we would do this:
  // int res = _open_osfhandle (_listenSocket.fileHandle, 0);
  // However, this opens a whole lot of problems and in general one should
  // never use _open_osfhandle for sockets.
  // Therefore, we do the following, although it has the potential to
  // lose the higher bits of the socket handle:
  int res = (int)_listenSocket.fileHandle;

  if (res == -1) {
    LOG(ERR) << "In ListenTask::setup could not convert socket handle to socket descriptor -- _open_osfhandle(...) failed";

    res = TRI_CLOSE_SOCKET(_listenSocket);

    if (res != 0) {
      res = WSAGetLastError();
      LOG(ERR) << "In ListenTask::setup closesocket(...) failed with error code: " << res;
    }

    TRI_invalidatesocket(&_listenSocket);
    return false;
  }

  _listenSocket.fileDescriptor = res;

#endif

  this->_scheduler = scheduler;
  this->_loop = loop;
  _readWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this,
                                               _listenSocket);

  return true;
}

void ListenTask::cleanup() {
  if (_scheduler != nullptr && _readWatcher != nullptr) {
    _scheduler->uninstallEvent(_readWatcher);
  }

  _readWatcher = nullptr;
}

bool ListenTask::handleEvent(EventToken token, EventType revents) {
  if (token == _readWatcher) {
    if ((revents & EVENT_SOCKET_READ) == 0) {
      return true;
    }

    static_assert(sizeof(sockaddr_in) <= sizeof(sockaddr_in6),
                  "expect sockaddr size to be less or equal to the v6 version");

    sockaddr_in6 addrmem;
    sockaddr_in* addr = (sockaddr_in*)&addrmem;
    socklen_t len = sizeof(sockaddr_in6);

    memset(addr, 0, sizeof(sockaddr_in6));

    // accept connection
    TRI_socket_t connectionSocket;
    connectionSocket = TRI_accept(_listenSocket, (sockaddr*)addr, &len);

    if (!TRI_isvalidsocket(connectionSocket)) {
      ++_acceptFailures;

      if (_acceptFailures < MAX_ACCEPT_ERRORS) {
        LOG(WARN) << "accept failed with " << errno << " (" << strerror(errno) << ")";
      } else if (_acceptFailures == MAX_ACCEPT_ERRORS) {
        LOG(ERR) << "too many accept failures, stopping logging";
      }

      return true;
    }

    _acceptFailures = 0;

    struct sockaddr_in6 addr_out_mem;
    struct sockaddr_in* addr_out = (sockaddr_in*)&addr_out_mem;
    socklen_t len_out = sizeof(addr_out_mem);

    int res = TRI_getsockname(connectionSocket, (sockaddr*)addr_out, &len_out);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_CLOSE_SOCKET(connectionSocket);

      LOG(WARN) << "getsockname failed with " << errno << " (" << strerror(errno) << ")";

      return true;
    }

    // disable nagle's algorithm, set to non-blocking and close-on-exec
    bool result = _endpoint->initIncoming(connectionSocket);

    if (!result) {
      TRI_CLOSE_SOCKET(connectionSocket);

      return true;
    }

    // set client address and port
    ConnectionInfo info;

    Endpoint::DomainType type = _endpoint->domainType();
    char host[NI_MAXHOST], serv[NI_MAXSERV];

    if (getnameinfo((sockaddr*)addr, len, host, sizeof(host), serv,
                    sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
      info.clientAddress = std::string(host);
      info.clientPort = ntohs(addr->sin_port);
    } else {
      if (type == Endpoint::DomainType::IPV4) {
        char buf[INET_ADDRSTRLEN + 1];
        char const* p =
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf) - 1);

        if (p != nullptr) {
          // cppcheck-suppress *
          buf[INET_ADDRSTRLEN] = '\0';
          info.clientAddress = p;
        }

        info.clientPort = ntohs(addr->sin_port);
      } else if (type == Endpoint::DomainType::IPV6) {
        char buf[INET6_ADDRSTRLEN + 1];
        char const* p =
            inet_ntop(AF_INET6, &addrmem.sin6_addr, buf, sizeof(buf) - 1);

        if (p != nullptr) {
          // cppcheck-suppress *
          buf[INET6_ADDRSTRLEN] = '\0';
          info.clientAddress = p;
        }

        info.clientPort = ntohs(addrmem.sin6_port);
      }
    }

    // set server address and port
    if (type == Endpoint::DomainType::IPV4) {
      char buf[INET_ADDRSTRLEN + 1];
      char const* p =
          inet_ntop(AF_INET, &addr_out->sin_addr, buf, sizeof(buf) - 1);

      if (p != nullptr) {
        // cppcheck-suppress *
        buf[INET_ADDRSTRLEN] = '\0';
        info.serverAddress = p;
      }

      info.serverPort = ntohs(addr_out->sin_port);
    } else if (type == Endpoint::DomainType::IPV6) {
      char buf[INET6_ADDRSTRLEN + 1];
      char const* p =
          inet_ntop(AF_INET6, &addr_out_mem.sin6_addr, buf, sizeof(buf) - 1);

      if (p != nullptr) {
        // cppcheck-suppress *
        buf[INET6_ADDRSTRLEN] = '\0';
        info.serverAddress = p;
      }

      info.serverPort = ntohs(addr_out_mem.sin6_port);
    } else {
      info.serverAddress = _endpoint->host();
      info.serverPort = _endpoint->port();
    }

    // set the endpoint
    info.endpoint = _endpoint->specification();
    info.endpointType = _endpoint->domainType();

    return handleConnected(connectionSocket, std::move(info));
  }

  return true;
}

// -----------------------------------------------------------------------------
// private methods
// -----------------------------------------------------------------------------

bool ListenTask::bindSocket() {
  _listenSocket = _endpoint->connect(30, 300);  // connect timeout in seconds

  if (!TRI_isvalidsocket(_listenSocket)) {
    return false;
  }

  return true;
}
