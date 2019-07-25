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

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "socket-utils.h"

#ifdef TRI_HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a socket
////////////////////////////////////////////////////////////////////////////////

int TRI_closesocket(TRI_socket_t s) {
  int res = TRI_ERROR_NO_ERROR;
#ifdef _WIN32
  if (s.fileHandle != TRI_INVALID_SOCKET) {
    res = shutdown(s.fileHandle, SD_SEND);

    if (res != 0) {
      // Windows complains about shutting down a socket that was not bound
      // so we will not print out the error here
      // LOG_TOPIC("52a7b", WARN, arangodb::Logger::FIXME) << "socket shutdown error: "
      // << WSAGetLastError();
    } else {
      char buf[256];
      int len;
      do {
        len = TRI_readsocket(s, buf, sizeof(buf), 0);
      } while (len > 0);
    }
    res = closesocket(s.fileHandle);

    if (res != 0) {
      LOG_TOPIC("f8bf5", WARN, arangodb::Logger::FIXME)
          << "socket close error: " << WSAGetLastError();
    }
  }
#else
  if (s.fileDescriptor != TRI_INVALID_SOCKET) {
    res = close(s.fileDescriptor);

    if (res == -1) {
      int myerrno = errno;
      LOG_TOPIC("977f8", WARN, arangodb::Logger::FIXME)
          << "socket close error: " << myerrno << ": " << strerror(myerrno);
    }
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
  res = send(s.fileHandle, (char const*)(buffer), (int)(numBytesToWrite), flags);
#else
  res = (int)write(s.fileDescriptor, buffer, numBytesToWrite);
#endif
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets close-on-exit for a socket
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_CLOSE_ON_EXEC

bool TRI_SetCloseOnExecSocket(TRI_socket_t s) { return true; }

#else

bool TRI_SetCloseOnExecSocket(TRI_socket_t s) {
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

bool TRI_SetNonBlockingSocket(TRI_socket_t s) {
  DWORD ul = 1;
  int res = ioctlsocket(s.fileHandle, FIONBIO, &ul);
  return (res == 0);
}

#else

bool TRI_SetNonBlockingSocket(TRI_socket_t s) {
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
/// @brief translates for IPv4 address
///
/// This code is copyright Internet Systems Consortium, Inc. ("ISC")
////////////////////////////////////////////////////////////////////////////////

int TRI_InetPton4(char const* src, unsigned char* dst) {
  static char const digits[] = "0123456789";

  int saw_digit, octets, ch;
  unsigned char tmp[sizeof(struct in_addr)], *tp;

  if (nullptr == src) {
    return TRI_ERROR_IP_ADDRESS_INVALID;
  }

  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;

  while ((ch = *src++) != '\0') {
    char const* pch;

    if ((pch = strchr(digits, ch)) != nullptr) {
      unsigned int nw = (unsigned int)(*tp * 10 + (pch - digits));

      if (saw_digit && *tp == 0) {
        return TRI_ERROR_IP_ADDRESS_INVALID;
      }

      if (nw > 255) {
        return TRI_ERROR_IP_ADDRESS_INVALID;
      }

      *tp = nw;

      if (!saw_digit) {
        if (++octets > 4) {
          return TRI_ERROR_IP_ADDRESS_INVALID;
        }

        saw_digit = 1;
      }
    } else if (ch == '.' && saw_digit) {
      if (octets == 4) {
        return TRI_ERROR_IP_ADDRESS_INVALID;
      }

      *++tp = 0;
      saw_digit = 0;
    } else {
      return TRI_ERROR_IP_ADDRESS_INVALID;
    }
  }

  if (octets < 4) {
    return TRI_ERROR_IP_ADDRESS_INVALID;
  }

  if (nullptr != dst) {
    memcpy(dst, tmp, sizeof(struct in_addr));
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translates for IPv6 address
///
/// This code is copyright Internet Systems Consortium, Inc. ("ISC")
////////////////////////////////////////////////////////////////////////////////

int TRI_InetPton6(char const* src, unsigned char* dst) {
  static char const xdigits_l[] = "0123456789abcdef";
  static char const xdigits_u[] = "0123456789ABCDEF";

  unsigned char tmp[sizeof(struct in6_addr)], *tp, *endp, *colonp;
  char const* curtok;
  int ch, seen_xdigits;
  unsigned int val;

  if (nullptr == src) {
    return TRI_ERROR_IP_ADDRESS_INVALID;
  }

  memset((tp = tmp), '\0', sizeof tmp);
  endp = tp + sizeof tmp;
  colonp = nullptr;

  /* Leading :: requires some special handling. */
  if (*src == ':') {
    if (*++src != ':') {
      return TRI_ERROR_IP_ADDRESS_INVALID;
    }
  }

  curtok = src;
  seen_xdigits = 0;
  val = 0;

  while ((ch = *src++) != '\0') {
    char const* pch;
    char const* xdigits;

    if ((pch = strchr((xdigits = xdigits_l), ch)) == nullptr) {
      pch = strchr((xdigits = xdigits_u), ch);
    }

    if (pch != nullptr) {
      val <<= 4;
      val |= (pch - xdigits);

      if (++seen_xdigits > 4) {
        return TRI_ERROR_IP_ADDRESS_INVALID;
      }

      continue;
    }

    if (ch == ':') {
      curtok = src;

      if (!seen_xdigits) {
        if (colonp) {
          return TRI_ERROR_IP_ADDRESS_INVALID;
        }

        colonp = tp;
        continue;
      } else if (*src == '\0') {
        return TRI_ERROR_IP_ADDRESS_INVALID;
      }

      if (tp + sizeof(uint16_t) > endp) {
        return TRI_ERROR_IP_ADDRESS_INVALID;
      }

      *tp++ = (unsigned char)(val >> 8) & 0xff;
      *tp++ = (unsigned char)val & 0xff;
      seen_xdigits = 0;
      val = 0;

      continue;
    }

    if (ch == '.' && ((tp + sizeof(struct in_addr)) <= endp)) {
      int err = TRI_InetPton4(curtok, tp);

      if (err == 0) {
        tp += sizeof(struct in_addr);
        seen_xdigits = 0;
        break; /*%< '\\0' was seen by inet_pton4(). */
      }
    }

    return TRI_ERROR_IP_ADDRESS_INVALID;
  }

  if (seen_xdigits) {
    if (tp + sizeof(uint16_t) > endp) {
      return TRI_ERROR_IP_ADDRESS_INVALID;
    }

    *tp++ = (unsigned char)(val >> 8) & 0xff;
    *tp++ = (unsigned char)val & 0xff;
  }

  if (colonp != nullptr) {
    /*
     * Since some memmove()'s erroneously fail to handle
     * overlapping regions, we'll do the shift by hand.
     */
    int const n = (int)(tp - colonp);
    int i;

    if (tp == endp) {
      return TRI_ERROR_IP_ADDRESS_INVALID;
    }

    for (i = 1; i <= n; i++) {
      endp[-i] = colonp[n - i];
      colonp[n - i] = 0;
    }

    tp = endp;
  }

  if (tp != endp) {
    return TRI_ERROR_IP_ADDRESS_INVALID;
  }

  if (nullptr != dst) {
    memcpy(dst, tmp, sizeof tmp);
  }

  return TRI_ERROR_NO_ERROR;
}

#ifdef _WIN32
bool TRI_setsockopttimeout(TRI_socket_t s, double timeout) {
  DWORD to = (DWORD)timeout * 1000;

  if (TRI_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char const*)&to, sizeof(to)) != 0) {
    return false;
  }

  if (TRI_setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char const*)&to, sizeof(to)) != 0) {
    return false;
  }
  return true;
}
#else
bool TRI_setsockopttimeout(TRI_socket_t s, double timeout) {
  struct timeval tv;

  // shut up Valgrind
  memset(&tv, 0, sizeof(tv));
  tv.tv_sec = (time_t)timeout;
  tv.tv_usec = (suseconds_t)((timeout - (double)tv.tv_sec) * 1000000.0);

  if (TRI_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    return false;
  }

  if (TRI_setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
    return false;
  }
  return true;
}
#endif
