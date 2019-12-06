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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_SOCKET__UTILS_H
#define ARANGODB_BASICS_SOCKET__UTILS_H 1

#include <stddef.h>

#include "Basics/Common.h"
#include "Basics/operating-system.h"

#ifdef TRI_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef TRI_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
#include <WS2tcpip.h>

typedef long suseconds_t;
#endif


////////////////////////////////////////////////////////////////////////////////
/// @brief socket types
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
struct TRI_socket_t {
  int fileDescriptor;
  SOCKET fileHandle;
};
#else
struct TRI_socket_t {
  int fileDescriptor;
};
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief socket abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline TRI_socket_t TRI_socket(int domain, int type, int protocol) {
  TRI_socket_t res;
#ifdef _WIN32
  res.fileHandle = socket(domain, type, protocol);
  res.fileDescriptor = -1;
#else
  res.fileDescriptor = socket(domain, type, protocol);
#endif
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief listen abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_listen(TRI_socket_t s, int backlog) {
#ifdef _WIN32
  return listen(s.fileHandle, backlog);
#else
  return listen(s.fileDescriptor, backlog);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bind abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_bind(TRI_socket_t s, const struct sockaddr* address, size_t addr_len) {
#ifdef _WIN32
  return bind(s.fileHandle, address, static_cast<int>(addr_len));
#else
  return bind(s.fileDescriptor, address, (socklen_t)addr_len);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_connect(TRI_socket_t s, const struct sockaddr* address, size_t addr_len) {
#ifdef _WIN32
  return connect(s.fileHandle, address, (int)addr_len);
#else
  return connect(s.fileDescriptor, address, (socklen_t)addr_len);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

static inline long TRI_send(TRI_socket_t s, const void* buffer, size_t length, int flags) {
#ifdef _WIN32
  return send(s.fileHandle, (char*)buffer, (int)length, flags);
#else
  return send(s.fileDescriptor, buffer, length, flags);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getsockopt abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static inline int TRI_getsockopt(TRI_socket_t s, int level, int optname,
                                 void* optval, socklen_t* optlen) {
  return getsockopt(s.fileHandle, level, optname, (char*)optval, optlen);
}
#else
static inline int TRI_getsockopt(TRI_socket_t s, int level, int optname,
                                 void* optval, socklen_t* optlen) {
  return getsockopt(s.fileDescriptor, level, optname, optval, optlen);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief setsockopt abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static inline int TRI_setsockopt(TRI_socket_t s, int level, int optname,
                                 const void* optval, int optlen) {
  return setsockopt(s.fileHandle, level, optname, (char const*)optval, optlen);
}
#else
static inline int TRI_setsockopt(TRI_socket_t s, int level, int optname,
                                 const void* optval, socklen_t optlen) {
  return setsockopt(s.fileDescriptor, level, optname, optval, optlen);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief setsockopt abstraction for different OSes
////////////////////////////////////////////////////////////////////////////////

bool TRI_setsockopttimeout(TRI_socket_t s, double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether or not a socket is valid
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_isvalidsocket(TRI_socket_t s) {
#ifdef _WIN32
  return s.fileHandle != TRI_INVALID_SOCKET;
#else
  return s.fileDescriptor != TRI_INVALID_SOCKET;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates a socket
////////////////////////////////////////////////////////////////////////////////

static inline void TRI_invalidatesocket(TRI_socket_t* s) {
#ifdef _WIN32
  s->fileHandle = TRI_INVALID_SOCKET;
  s->fileDescriptor = -1;
#else
  s->fileDescriptor = TRI_INVALID_SOCKET;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get file descriptor or handle, depending on OS
///
/// Note that this returns the fileHandle under Windows which is exactly
/// the right thing we need in all but one places.
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static inline SOCKET TRI_get_fd_or_handle_of_socket(TRI_socket_t s) {
  return s.fileHandle;
}
#else
static inline int TRI_get_fd_or_handle_of_socket(TRI_socket_t s) {
  return s.fileDescriptor;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open socket
////////////////////////////////////////////////////////////////////////////////

int TRI_closesocket(TRI_socket_t);

int TRI_readsocket(TRI_socket_t, void* buffer, size_t numBytesToRead, int flags);

int TRI_writesocket(TRI_socket_t, const void* buffer, size_t numBytesToWrite, int flags);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets non-blocking mode for a socket
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetNonBlockingSocket(TRI_socket_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets close-on-exec for a socket
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetCloseOnExecSocket(TRI_socket_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief translates for IPv4 address
///
/// This code is copyright Internet Systems Consortium, Inc. ("ISC")
////////////////////////////////////////////////////////////////////////////////

int TRI_InetPton4(char const* src, unsigned char* dst);

////////////////////////////////////////////////////////////////////////////////
/// @brief translates for IPv6 address
///
/// This code is copyright Internet Systems Consortium, Inc. ("ISC")
////////////////////////////////////////////////////////////////////////////////

int TRI_InetPton6(char const* src, unsigned char* dst);

#endif
