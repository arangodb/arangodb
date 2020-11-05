////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "thread_utils.hpp"

#include <memory>
#include <cstring>

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <sys/prctl.h>
#endif

namespace iresearch {

bool set_thread_name(const thread_name_t name) noexcept {
#ifdef _WIN32
  #if _WIN32_WINNT >= _WIN32_WINNT_WIN10
    return SUCCEEDED(SetThreadDescription(GetCurrentThread(), name));
  #else
    return false;
  #endif
#else
  return 0 == prctl(PR_SET_NAME, name, 0, 0, 0);
#endif
}

bool get_thread_name(std::basic_string<std::remove_pointer_t<thread_name_t>>& name) {
#ifdef _WIN32
  #if (_WIN32_WINNT >= _WIN32_WINNT_WIN10
    std::unique_ptr<void*, ::LocalFree> guard;
    thread_name_t tmp;
    auto const res = GetThreadDescription(GetCurrentThread(), &tmp);
    guard.reset(tmp);
    if (SUCCEEDED(res)) {
      name = tmp;
      return true;
    }
    return false;
  #else
    return false;
  #endif
#else
    name.resize(16, 0); // posix restricts name length to 16 bytes
    if (0 == prctl(PR_GET_NAME, const_cast<char*>(name.data()), 0, 0, 0)) {
      name.resize(std::strlen(name.c_str()));
      return true;
    }
    return false;
#endif
}

}
