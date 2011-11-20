////////////////////////////////////////////////////////////////////////////////
/// @brief collection of socket functions
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
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SocketUtils.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <Basics/Logger.h>
#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>

namespace triagens {
  namespace basics {
    namespace SocketUtils {
      static Mutex gethostbynameLock;



      char* gethostbyname (string const& hostname, size_t& length) {
        MUTEX_LOCKER(gethostbynameLock);

        struct ::hostent * sheep = ::gethostbyname(const_cast<char*> (hostname.c_str()));

        if (sheep == 0) {
          LOGGER_WARNING << "cannot resolve hostname '" << hostname << "'";
          return 0;
        }

        length = sheep->h_length;
        char* netaddress = new char[length];
        memcpy(netaddress, sheep->h_addr, length);

        return netaddress;
      }



      bool setNonBlocking (socket_t fd) {
#ifdef TRI_HAVE_WIN32_NON_BLOCKING
        DWORD ul = 1;

        return ioctlsocket(fd, FIONBIO, &ul) == SOCKET_ERROR ? false : true;
#else
        long flags = fcntl(fd, F_GETFL, 0);

        if (flags < 0) {
          return false;
        }

        flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        if (flags < 0) {
          return false;
        }

        return true;
#endif
      }



      bool setCloseOnExec (socket_t fd) {
#ifdef TRI_HAVE_WIN32_CLOSE_ON_EXEC
#else
        long flags = fcntl(fd, F_GETFD, 0);

        if (flags < 0) {
          return false;
        }

        flags = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

        if (flags < 0) {
          return false;
        }
#endif

        return true;
      }
    }
  }
}

