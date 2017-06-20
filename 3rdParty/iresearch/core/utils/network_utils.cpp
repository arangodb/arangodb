//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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

NS_ROOT

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

NS_END // ROOT
