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

#include "ConnectionTask.h"

#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "BasicsC/socket-utils.h"
#include "Logger/Logger.h"

#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

ConnectionTask::ConnectionTask (string const& hostname, int port)
  : Task("ConnectionTask"),
    SocketTask(0),
    watcher(0),
    state(STATE_UNCONNECTED),
    idle(true),
    connectTimeout(0.0),
    commTimeout(0.0),
    hostname(hostname),
    port(port),
    netaddress(0),
    netaddressLength(0),
    resolved(false) {
  resolveAddress();
  connectSocket();
}



ConnectionTask::ConnectionTask (string const& hostname, int port, double connectTimeout)
  : Task("ConnectionTask"),
    SocketTask(0),
    watcher(0),
    state(STATE_UNCONNECTED),
    idle(true),
    connectTimeout(connectTimeout),
    commTimeout(0.0),
    hostname(hostname),
    port(port),
    netaddress(0),
    netaddressLength(0),
    resolved(false) {
  resolveAddress();
  connectSocket();
}



ConnectionTask::ConnectionTask (string const& hostname, int port, double connectTimeout, double commTimeout)
  : Task("ConnectionTask"),
    SocketTask(0),
    watcher(0),
    state(STATE_UNCONNECTED),
    idle(true),
    connectTimeout(connectTimeout),
    commTimeout(commTimeout),
    hostname(hostname),
    port(port),
    netaddress(0),
    netaddressLength(0),
    resolved(false) {
  resolveAddress();
  connectSocket();
}



ConnectionTask::ConnectionTask (socket_t fd)
  : Task("ConnectionTask"),
    SocketTask(fd),
    watcher(0),
    state(STATE_CONNECTED),
    idle(true),
    connectTimeout(0.0),
    commTimeout(0.0),
    hostname(""),
    port(0),
    netaddress(0),
    netaddressLength(0),
    resolved(true) {
}



ConnectionTask::ConnectionTask (socket_t fd, double commTimeout)
  : Task("ConnectionTask"),
    SocketTask(fd),
    watcher(0),
    state(STATE_CONNECTED),
    idle(true),
    connectTimeout(0.0),
    commTimeout(commTimeout),
    hostname(""),
    port(0),
    netaddress(0),
    netaddressLength(0),
    resolved(true) {
}



ConnectionTask::~ConnectionTask () {
  if (netaddress != 0) {
    delete[] netaddress;
  }
  
  if (state == STATE_CONNECTION_INPROGRESS) {
    LOGGER_TRACE << "closing inprogrss socket " << commSocket;
    int r = close(commSocket);
    
    if (r < 0 ) {
      LOGGER_ERROR << "close failed with " << errno << " (" << strerror(errno) << ") on " << commSocket;
    }
    
    commSocket = -1;
  }
  
  else if (state == STATE_CONNECTED) {
    LOGGER_TRACE << "closing connected socket " << commSocket;
    int r = close(commSocket);
    
    if (r < 0 ) {
      LOGGER_ERROR << "close failed with " << errno << " (" << strerror(errno) << ") on " << commSocket;
    }
    
    commSocket = -1;
  }
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

void ConnectionTask::setup (Scheduler* scheduler, EventLoop loop) {
  this->scheduler = scheduler;
  this->loop = loop;
  
  // we must have resolved the hostname
  if (! resolved) {
    return;
  }
  
  // and started the connection process
  if (! isConnecting()) {
    return;
  }
  
  // initialize all watchers
  SocketTask::setup(scheduler, loop);

  // we are already connected
  if (isConnected()) {
    LOGGER_TRACE << "already connected, waiting for requests on socket " << commSocket;
    watcher = 0;
  }
  
  // we are in the process of connecting
  else {
    scheduler->uninstallEvent(readWatcher);
    readWatcher = 0;
    
    if (0.0 < connectTimeout) {
      watcher = scheduler->installTimerEvent(loop, this, connectTimeout);
    }
    else {
      watcher = 0;
    }
    
    LOGGER_TRACE << "waiting for connect to complete on socket " << commSocket;
  }
}



void ConnectionTask::cleanup () {
  SocketTask::cleanup();
  
  scheduler->uninstallEvent(watcher);
}



bool ConnectionTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;
  
  // upps, should not happen
  if (state == STATE_UNCONNECTED) {
    LOGGER_WARNING << "received event in unconnected state";
  }
  
  // we are trying to connect
  else if (state == STATE_CONNECTION_INPROGRESS) {
    result = handleConnectionEvent(token, revents);
  }
  
  // we are connected
  else if (state == STATE_CONNECTED) {
    result = handleCommunicationEvent(token, revents);
  }
  
  return result;
}

// -----------------------------------------------------------------------------
// private methods
// -----------------------------------------------------------------------------

bool ConnectionTask::handleConnectionEvent (EventToken token, EventType revents) {
  
  // we received an event on the write socket
  if (token == writeWatcher && (revents & EVENT_SOCKET_WRITE)) {
    scheduler->uninstallEvent(readWatcher);
    scheduler->uninstallEvent(writeWatcher);
    scheduler->uninstallEvent(watcher);
    
    int valopt;
    socklen_t lon = sizeof(valopt);
    
    if (getsockopt(commSocket, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) {
      LOGGER_ERROR << "getsockopt failed with " << errno << " (" << strerror(errno) << ") on " << commSocket;
      
      close(commSocket);
      commSocket = -1;
      state = STATE_UNCONNECTED;
      
      return handleConnectionFailure();
    }
    
    if (valopt != 0) {
      LOGGER_DEBUG << "delayed connect failed with " << valopt << " (" << strerror(valopt) << ") on " << commSocket;
      
      close(commSocket);
      commSocket = -1;
      state = STATE_UNCONNECTED;
      
      return handleConnectionFailure();
    }
    else {
      state = STATE_CONNECTED;
      
      readWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this, commSocket);
      writeWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this, commSocket);
      
      if (0.0 < commTimeout) {
        watcher = scheduler->installTimerEvent(loop, this, commTimeout);
      }
      else {
        watcher = 0;
      }
      
      LOGGER_TRACE << "connection completed on socket " << commSocket;
      
      return handleConnected();
    }
  }
  
  // we received a timer event
  if (token == watcher && (revents & EVENT_TIMER)) {
    close(commSocket);
    commSocket = -1;
    state = STATE_UNCONNECTED;
    
    return handleConnectionTimeout();
  }
  
  return true;
}



bool ConnectionTask::handleCommunicationEvent (EventToken token, EventType revents) {
  
  // we received a timer event
  if (token == watcher && (revents & EVENT_TIMER)) {
    if (idle) {
      if (watcher != 0) {
        LOGGER_TRACE << "clearing timer because we are idle";

        scheduler->clearTimer(watcher);
      }
    }
    else if (0.0 < commTimeout) {
      LOGGER_TRACE << "received a timer event";
      
      return handleCommunicationTimeout();
    }
  }
  
  if (revents & (EVENT_SOCKET_READ | EVENT_SOCKET_WRITE)) {
    if (0.0 < commTimeout) {
      scheduler->rearmTimer(watcher, commTimeout);
    }
  }
  
  return SocketTask::handleEvent(token, revents);
}



void ConnectionTask::resolveAddress () {
  resolved = false;
  
  if (netaddress != 0) {
    delete[] netaddress;
  }
  
  netaddress = TRI_GetHostByName(hostname.c_str(), &netaddressLength);
  
  if (netaddress == 0) {
    LOGGER_ERROR << "cannot resolve hostname '" << hostname << "'";
    return;
  }
  
  resolved = true;
}



void ConnectionTask::connectSocket () {
  if (! resolved) {
    LOGGER_DEBUG << "connect failed beacuse hostname cannot be resolved";
    
    state = STATE_UNCONNECTED;
    return;
  }
  
  // close open socket from last run
  if (state != STATE_UNCONNECTED) {
    close(commSocket);
    commSocket = -1;
  }
  
  state = STATE_UNCONNECTED;
  
  // open a socket
  commSocket = ::socket(AF_INET, SOCK_STREAM, 0);
  
  if (commSocket < 0) {
    LOGGER_ERROR << "socket failed with " << errno << " (" << strerror(errno) << ") on " << commSocket;
    
    return;
  }
  
  LOGGER_TRACE << "created socket " << commSocket;
  
  // we are in the process of connecting the socket
  state = STATE_CONNECTION_INPROGRESS;
  
  // use non-blocking io
  TRI_SetNonBlockingSocket(commSocket);
  
  // and try to connect the socket to the given address
  struct sockaddr_in saddr;
  
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  
  if (sizeof(saddr.sin_addr.s_addr) < netaddressLength) {
    close(commSocket);
    commSocket = -1;
    state = STATE_UNCONNECTED;
    
    LOGGER_ERROR << "IPv6 address are not allowed";
    
    return;
  }
  
  memcpy(&(saddr.sin_addr.s_addr), netaddress, netaddressLength);
  saddr.sin_port = htons(port);
  
  int res = ::connect(commSocket, reinterpret_cast<struct sockaddr*>(&saddr), sizeof(saddr));
  
  if (res < 0) {
    if (errno == EINPROGRESS) {
      LOGGER_TRACE << "connection to '" << hostname << "' is in progress on " << commSocket;
      
      return;
    }
    else {
      close(commSocket);
      commSocket = -1;
      state = STATE_UNCONNECTED;
      
      LOGGER_ERROR << "connect failed with " << errno << " (" << strerror(errno) << ") on " << commSocket;
          
      return;
    }
  }
  
  state = STATE_CONNECTED;
  handleConnected();
}
