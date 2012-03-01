////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to establish connections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2008-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ListenTask.h"

#include <errno.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "BasicsC/socket-utils.h"
#include "Logger/Logger.h"

#include "GeneralServer/GeneralFigures.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;

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

ListenTask::ListenTask (struct addrinfo *aip, bool reuseAddress)
  : Task("ListenTask"),
    readWatcher(0),
    reuseAddress(reuseAddress),
    address(""),
    port(0),
    listenSocket(0),
    bound(false),
    acceptFailures(0) {
  bindSocket(aip);
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
/*
        info.serverAddress = inet_ntoa(addr_out.sin_addr);
        info.serverPort = port;
        
        info.clientAddress = inet_ntoa(addr.sin_addr);
        info.clientPort = addr.sin_port;
*/
    // set client address and port
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
    
    info.serverAddress = address;
    info.serverPort = port;
    
    return handleConnected(connfd, info);
  }
  
  return true;
}

// -----------------------------------------------------------------------------
// private methods
// -----------------------------------------------------------------------------

bool ListenTask::bindSocket () {
  bound = false;
  listenSocket = -1;
  
  struct addrinfo *result, *aip;
  struct addrinfo hints;
  int error;
  
  memset(&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ALL;
  hints.ai_socktype = SOCK_STREAM;
  
  string portString = StringUtils::itoa(port);
  
  if (address.empty()) {
    LOGGER_ERROR << "get any address";
    error = getaddrinfo(NULL, portString.c_str(), &hints, &result);
  }
  else {
    error = getaddrinfo(address.c_str(), portString.c_str(), &hints, &result);
  }
  
  if (error != 0) {
    LOGGER_ERROR << "getaddrinfo for host: " << address.c_str() << " => " << gai_strerror(error);
    return false;
  }
  
  // Try all returned addresses until one works
  for (aip = result; aip != NULL; aip = aip->ai_next) {
    
    // try to bind the address info pointer
    if (bindSocket(aip)) {
      // OK
      break;
    }
    
  }
  
  freeaddrinfo(result);
  
  return bound;
}



bool ListenTask::bindSocket (struct addrinfo *aip) {
  bound = false;
  listenSocket = -1;
  
  // set address and port
  char host[NI_MAXHOST], serv[NI_MAXSERV];
  if (getnameinfo(aip->ai_addr, aip->ai_addrlen,
                  host, sizeof(host),
                  serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    
    address = string(host);
    port = StringUtils::int32(string(serv));
    
    LOGGER_TRACE << "bind to address '" << address << "' port '" << string(serv) << "'";
  }
  
  listenSocket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
  if (listenSocket == -1) {
    return false;
  }
  
  // try to reuse address
  if (reuseAddress) {
    int opt = 1;
    
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*> (&opt), sizeof (opt)) == -1) {
      LOGGER_ERROR << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";
      return false;
    }
    
    LOGGER_TRACE << "reuse address flag set";
  }
  
  int res = bind(listenSocket, aip->ai_addr, aip->ai_addrlen);
  if (res < 0) {
    LOGGER_ERROR << "bind failed with " << errno << " (" << strerror(errno) << ")";
    (void) close(listenSocket);
    listenSocket = -1;
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

  if (!ok) {
    close(listenSocket);
    
    LOGGER_ERROR << "cannot switch to non-blocking: " << errno << " (" << strerror(errno) << ")";
    
    return false;
  }

  // set close on exit
  ok = TRI_SetCloseOnExecSocket(listenSocket);

  if (!ok) {
    close(listenSocket);
    
    LOGGER_ERROR << "cannot set close-on-exit: " << errno << " (" << strerror(errno) << ")";
    
    return false;
  }
  
  bound = true;
  
  return true;
}
