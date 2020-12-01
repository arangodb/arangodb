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

#include <cstring>

#ifdef __linux__

#include <sys/prctl.h>
#include <memory>

namespace iresearch {

bool set_thread_name(const thread_name_t name) noexcept {
  return 0 == prctl(PR_SET_NAME, name, 0, 0, 0);
}

bool get_thread_name(std::basic_string<std::remove_pointer_t<thread_name_t>>& name) {
  name.resize(16, 0); // posix restricts name length to 16 bytes
  if (0 == prctl(PR_GET_NAME, const_cast<char*>(name.data()), 0, 0, 0)) {
    name.resize(std::strlen(name.c_str()));
    return true;
  }
  return false;
}

} // iresearch

#elif defined(_WIN32) && (_WIN32_WINNT >= _WIN32_WINNT_WIN10)

#include <processthreadsapi.h>

namespace iresearch {

bool set_thread_name(const thread_name_t name) noexcept {
  return SUCCEEDED(SetThreadDescription(GetCurrentThread(), name));
}

bool get_thread_name(std::basic_string<std::remove_pointer_t<thread_name_t>>& name) {
  std::unique_ptr<void*, ::LocalFree> guard;
  thread_name_t tmp;
  auto const res = GetThreadDescription(GetCurrentThread(), &tmp);
  guard.reset(tmp);
  if (SUCCEEDED(res)) {
    name = tmp;
    return true;
  }
  return false;
}

} // iresearch

#else

namespace iresearch {

bool set_thread_name(const thread_name_t name) noexcept {
  return false;
}

bool get_thread_name(std::basic_string<std::remove_pointer_t<thread_name_t>>&) noexcept {
  return false;
}

} // iresearch

#endif
