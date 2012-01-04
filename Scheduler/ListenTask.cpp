////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to establish connections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ListenTask.h"

#include <errno.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <Basics/MutexLocker.h>
#include <BasicsC/socket-utils.h>
#include <Logger/Logger.h>

#include "GeneralServer/GeneralFigures.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ListenTask::ListenTask (string const& address, int port, bool reuseAddress)
      : Task("ListenTask"),
        readWatcher(0),
        reuseAddress(reuseAddress),
        address(address),
        port(port),
        listenSocket(0),
        bound(false),
        acceptFailures(0) {
      bindSocket();
    }



    ListenTask::ListenTask (int port, bool reuseAddress)
      : Task("ListenTask"),
        readWatcher(0),
        reuseAddress(reuseAddress),
        address(""),
        port(port),
        listenSocket(0),
        bound(false),
        acceptFailures(0) {
      bindSocket();
    }



    ListenTask::~ListenTask () {
      close(listenSocket);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool ListenTask::isBound () const {
      MUTEX_LOCKER(changeLock);

      return bound;
    }



    bool ListenTask::rebind () {
      MUTEX_LOCKER(changeLock);

      if (bound) {
        close(listenSocket);
      }

      return bindSocket();
    }

    // -----------------------------------------------------------------------------
    // Task methods
    // -----------------------------------------------------------------------------

    void ListenTask::setup (Scheduler* scheduler, EventLoop loop) {
      if (! bound) {
        return;
      }

      this->scheduler = scheduler;
      this->loop = loop;

      readWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this, listenSocket);
    }



    void ListenTask::cleanup () {
      scheduler->uninstallEvent(readWatcher);
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
        socket_t connfd = accept(listenSocket, (sockaddr*) &addr, &len);

        if (connfd == INVALID_SOCKET) {
          ++acceptFailures;

          if (acceptFailures < MAX_ACCEPT_ERRORS) {
            LOGGER_WARNING << "accept failed with " << errno << " (" << strerror(errno) << ")";
          }
          else if (acceptFailures == MAX_ACCEPT_ERRORS) {
            LOGGER_ERROR << "too many accept failures, stopping logging";
          }

          GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

          return true;
        }

        struct sockaddr_in addr_out;
        socklen_t len_out = sizeof(addr_out);

        int res = getsockname(connfd, (sockaddr*) &addr_out, &len_out);

        if (res != 0) {
          close(connfd);

          LOGGER_WARNING << "getsockname failed with " << errno << " (" << strerror(errno) << ")";
          GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

          return true;
        }

        acceptFailures = 0;

        // disable nagle's algorithm
        int n = 1;
        res = setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char*)&n, sizeof(n));

        if (res != 0 ) {
          close(connfd);

          LOGGER_WARNING << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";
          GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

          return true;
        }

        // set socket to non-blocking
        bool ok = TRI_SetNonBlockingSocket(connfd);

        if (! ok) {
          close(connfd);

          LOGGER_ERROR << "cannot switch to non-blocking: " << errno << " (" << strerror(errno) << ")";
          GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

          return true;
        }

        ok = TRI_SetCloseOnExecSocket(connfd);

        if (! ok) {
          close(connfd);

          LOGGER_ERROR << "cannot set close-on-exec: " << errno << " (" << strerror(errno) << ")";
          GeneralFigures::incCounter<GeneralFigures::GeneralServerStatistics::connectErrorsAccessor>();

          return true;
        }

        // handle connection
        ConnectionInfo info;

        info.serverAddress = inet_ntoa(addr_out.sin_addr);
        info.serverPort = port;

        info.clientAddress = inet_ntoa(addr.sin_addr);
        info.clientPort = addr.sin_port;

        return handleConnected(connfd, info);
      }

      return true;
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    bool ListenTask::bindSocket () {
      bound = false;

      // create a new socket
      listenSocket  = socket(AF_INET, SOCK_STREAM, 0);

      if (listenSocket < 0) {
        LOGGER_ERROR << "socket failed with " << errno << " (" << strerror(errno) << ")";
        return false;
      }

      // try to reuse address
      if (reuseAddress) {
        int opt = 1;

        if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) == -1) {
          LOGGER_ERROR << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";
          return false;
        }

        LOGGER_TRACE << "reuse address flag set";
      }

      // bind to any address
      sockaddr_in addr;

      if (address.empty()) {
        memset(&addr, 0, sizeof(addr));

        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(port);
      }

      // bind to a given address
      else {
        size_t length;
        char* a = TRI_GetHostByName(address.c_str(), &length);

        if (a == 0) {
          LOGGER_ERROR << "cannot resolve hostname: " << errno << " (" << strerror(errno) << ")";
          return false;
        }

        if (sizeof(addr.sin_addr.s_addr) < length) {
          LOGGER_ERROR << "IPv6 address are not allowed";
          return false;
        }

        // bind socket to an address
        memset(&addr, 0, sizeof(addr));

        addr.sin_family = AF_INET;
        memcpy(&(addr.sin_addr.s_addr), a, length);
        addr.sin_port = htons(port);

        delete[] a;
      }

      int res = bind(listenSocket, (const sockaddr*) &addr, sizeof(addr));

      if (res < 0) {
        close(listenSocket);

        LOGGER_ERROR << "bind failed with " << errno << " (" << strerror(errno) << ")";

        return false;
      }

      // listen for new connection
      res = listen(listenSocket, 10);

      if (res < 0) {
        close(listenSocket);

        LOGGER_ERROR << "listen failed with " << errno << " (" << strerror(errno) << ")";

        return false;
      }

      // set socket to non-blocking
      bool ok = TRI_SetNonBlockingSocket(listenSocket);

      if (! ok) {
        close(listenSocket);

        LOGGER_ERROR << "cannot switch to non-blocking: " << errno << " (" << strerror(errno) << ")";

        return false;
      }

      // set close on exit
      ok = TRI_SetCloseOnExecSocket(listenSocket);

      if (! ok) {
        close(listenSocket);

        LOGGER_ERROR << "cannot set close-on-exit: " << errno << " (" << strerror(errno) << ")";

        return false;
      }

      bound = true;

      return true;
    }
  }
}
