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

#ifndef TRIAGENS_BASICS_C_SOCKET_UTILS_H
#define TRIAGENS_BASICS_C_SOCKET_UTILS_H 1

#include "BasicsC/common.h"

#ifdef TRI_HAVE_LINUX_SOCKETS
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/file.h>
#endif

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Sockets
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid socket
////////////////////////////////////////////////////////////////////////////////

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif


////////////////////////////////////////////////////////////////////////////////
/// @brief socket types
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
  typedef struct TRI_socket_s {
    int fileDescriptor;
    SOCKET fileHandle;
  } TRI_socket_t;
#else
  typedef struct TRI_socket_s {
    int fileDescriptor;
  } TRI_socket_t;
#endif


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

////////////////////////////////////////////////////////////////////////////////
/// @brief socket abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline TRI_socket_t TRI_socket (int domain, int type, int protocol) {
  TRI_socket_t res;
#ifdef _WIN32
  res.fileHandle = socket(domain, type, protocol);
  res.fileDescriptor = INVALID_SOCKET;
#else
  res.fileDescriptor = socket(domain, type, protocol);
#endif
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief listen abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_listen (TRI_socket_t socket, int backlog) {
#ifdef _WIN32
  return listen(socket.fileHandle, backlog);
#else
  return listen(socket.fileDescriptor, backlog);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief accept abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline TRI_socket_t TRI_accept (TRI_socket_t socket, struct sockaddr* address,
                                socklen_t* address_len) {
  TRI_socket_t res;
#ifdef _WIN32
  res.fileHandle = accept(socket.fileHandle, address, address_len);
  res.fileDescriptor = INVALID_SOCKET;
#else
  res.fileDescriptor = accept(socket.fileDescriptor, address, address_len);
#endif
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bind abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_bind (TRI_socket_t socket, const struct sockaddr* address, 
              int addr_len) {
#ifdef _WIN32
  return bind(socket.fileHandle, address, addr_len);
#else
  return bind(socket.fileDescriptor, address, addr_len);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_connect (TRI_socket_t socket, const struct sockaddr *address, int addr_len) {
#ifdef _WIN32
  return connect(socket.fileHandle, address, addr_len);
#else
  return connect(socket.fileDescriptor, address, addr_len);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_send (TRI_socket_t socket, const void* buffer, size_t length,
                     int flags) {
#ifdef _WIN32
  return send(socket.fileHandle, (char*) buffer, (int) length, flags);
#else
  return send(socket.fileDescriptor, buffer, length, flags);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getsockname abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static inline int TRI_getsockname (TRI_socket_t socket, struct sockaddr* addr, 
                            int* len) {
  return getsockname(socket.fileHandle, addr, len);
}
#else
static inline int TRI_getsockname (TRI_socket_t socket, struct sockaddr* addr, 
                            socklen_t* len) {
  return getsockname(socket.fileDescriptor, addr, len);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief getsockopt abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static inline int TRI_getsockopt (TRI_socket_t socket, int level, int optname,
                           void* optval, socklen_t* optlen) {
  return getsockopt(socket.fileHandle, level, optname, (char*) optval, optlen);
}
#else
static inline int TRI_getsockopt (TRI_socket_t socket, int level, int optname,
                           void* optval, socklen_t* optlen) {
  return getsockopt(socket.fileDescriptor, level, optname, optval, optlen);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief setsockopt abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static inline int TRI_setsockopt (TRI_socket_t socket, int level, int optname,
                           const void* optval, int optlen) {
  return setsockopt(socket.fileHandle, level, optname, (const char*) optval, optlen);
}
#else
static inline int TRI_setsockopt (TRI_socket_t socket, int level, int optname,
                           const void* optval, socklen_t optlen) {
  return setsockopt(socket.fileDescriptor, level, optname, optval, optlen);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether or not a socket is valid
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_isvalidsocket (TRI_socket_t socket) {
#ifdef _WIN32
  return socket.fileHandle != INVALID_SOCKET;
#else
  return socket.fileDescriptor != INVALID_SOCKET;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates a socket
////////////////////////////////////////////////////////////////////////////////

static inline void TRI_invalidatesocket (TRI_socket_t* socket) {
#ifdef _WIN32
  socket->fileHandle = INVALID_SOCKET;
  socket->fileDescriptor = INVALID_SOCKET;
#else
  socket->fileDescriptor = INVALID_SOCKET;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get file descriptor or handle, depending on OS
///
/// Note that this returns the fileHandle under Windows which is exactly
/// the right thing we need in all but one places.
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_get_fd_or_handle_of_socket (TRI_socket_t socket) {
#ifdef _WIN32
  return (int)(socket.fileHandle);
#else
  return socket.fileDescriptor;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open socket
////////////////////////////////////////////////////////////////////////////////

int TRI_closesocket (TRI_socket_t);

int TRI_readsocket (TRI_socket_t, void* buffer, size_t numBytesToRead, int flags);

int TRI_writesocket (TRI_socket_t, const void* buffer, size_t numBytesToWrite, int flags);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets non-blocking mode for a socket
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetNonBlockingSocket (TRI_socket_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets close-on-exit for a socket
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetCloseOnExitSocket (TRI_socket_t);

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

void TRI_InitialiseSockets (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the sockets components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownSockets (void);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
