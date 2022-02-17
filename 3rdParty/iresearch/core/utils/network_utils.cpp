////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "network_utils.hpp"
#include "log.hpp"

#ifdef _WIN32
  #include <WinSock2.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <unistd.h>
#endif // _WIN32

#include <cstring>

namespace iresearch {

#ifdef _WIN32
  #pragma warning(disable : 4706)
#endif // _WIN32

int get_host_name(char* name, size_t size) {
#ifdef _WIN32
  WSADATA wsaData;
  int err;

  if (err = WSAStartup(MAKEWORD(2, 2), &wsaData)) {
    IR_FRMT_ERROR("WSAStartup failed with error: %d", err);
    return -1;
  }
  
  if (err = ::gethostname(name, static_cast<int>(size))) {
    err = ::WSAGetLastError();
  }

  WSACleanup();
  return err;
#else
  return gethostname(name, size);
#endif // _WIN32
}

#ifdef _WIN32
  #pragma warning(default: 4706)
#endif // _WIN32

bool is_same_hostname(const char* rhs, size_t size) {
  char name[256] = {};
  if (const int err = get_host_name(name, sizeof name)) {
    IR_FRMT_ERROR("Unable to get hostname, error: %d", err);
    return false;
  }

  if (std::strlen(name) != size) {
    return false;
  }

  return std::equal(name, name + size, rhs);
}

} // ROOT
