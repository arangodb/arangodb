////////////////////////////////////////////////////////////////////////////////
/// @brief collection of socket functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "socket-utils.h"

#ifdef TRI_HAVE_LINUX_SOCKETS
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "BasicsC/logging.h"
#include "BasicsC/locks.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Sockets
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Sockets
/// @{
////////////////////////////////////////////////////////////////////////////////

int TRI_closesocket(TRI_socket_t s) {
  int res = 0;
  // if this is the last file descriptor associated with the open file
  // I really hope the resources are released under both windows and linux
  #ifdef _WIN32
/*
    if (s.fileHandle > 0) {
      res = shutdown(s.fileHandle,2);
      res = closesocket(s.fileHandle);
    }
*/
    if (s.fileDescriptor > 0) {
      res = _close(s.fileDescriptor);
    }
  #else
    if (s.fileDescriptor > 0) {
      res = close(s.fileDescriptor);
    }
  #endif
  return res;
}


int TRI_readsocket(TRI_socket_t s, void* buffer, size_t numBytesToRead, int flags) {
  int res;
  #ifdef _WIN32
    res = recv(s.fileHandle, (char*)(buffer), (int)(numBytesToRead), flags);
  #else
    res = read(s.fileDescriptor, buffer, numBytesToRead);
  #endif
  return res;
}


int TRI_writesocket(TRI_socket_t s, const void* buffer, size_t numBytesToWrite, int flags) {
  int res;
  #ifdef _WIN32
    res = send(s.fileHandle, (const char*)(buffer), (int)(numBytesToWrite), flags);
  #else
    res = (int)(write(s.fileHandle, buffer, numBytesToWrite));
  #endif
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets close-on-exit for a socket
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_CLOSE_ON_EXEC

bool TRI_SetCloseOnExitSocket (TRI_socket_t s) {
  return true;
}

#else

bool TRI_SetCloseOnExitSocket (TRI_socket_t s) {
  long flags = fcntl(s.fileDescriptor, F_GETFD, 0);

  if (flags < 0) {
    return false;
  }

  flags = fcntl(s.fileDescriptor, F_SETFD, flags | FD_CLOEXEC);

  if (flags < 0) {
    return false;
  }

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets non-blocking mode for a socket
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_NON_BLOCKING

bool TRI_SetNonBlockingSocket (TRI_socket_t s) {
  int res;
  DWORD ul = 1;
  res = ioctlsocket(s.fileHandle, FIONBIO, &ul);
  return (res != INVALID_SOCKET);
}

#else

bool TRI_SetNonBlockingSocket (TRI_socket_t s) {
  long flags = fcntl(s.fileDescriptor, F_GETFL, 0);

  if (flags < 0) {
    return false;
  }

  flags = fcntl(s.fileDescriptor, F_SETFL, flags | O_NONBLOCK);

  if (flags < 0) {
    return false;
  }

  return true;
}

#endif


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Sockets
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the sockets components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseSockets (void) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the sockets components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownSockets (void) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
