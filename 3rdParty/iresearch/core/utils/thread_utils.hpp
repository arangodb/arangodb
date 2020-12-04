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

#ifndef IRESEARCH_THREAD_UTILS_H
#define IRESEARCH_THREAD_UTILS_H

#include <mutex>

#include "shared.hpp"

#ifdef _WIN32
  #define thread_name_t wchar_t*
#else
  #define thread_name_t char*
#endif

namespace iresearch {

template<typename Mutex>
[[nodiscard]] inline std::unique_lock<Mutex> make_unique_lock(Mutex& mtx) {
  return std::unique_lock<Mutex>(mtx);
}

template<typename Mutex, typename Mode>
[[nodiscard]] inline std::unique_lock<Mutex> make_unique_lock(Mutex& mtx, Mode mode) {
  return std::unique_lock<Mutex>(mtx, mode);
}

template<typename Mutex>
[[nodiscard]] inline std::lock_guard<Mutex> make_lock_guard(Mutex& mtx) {
  return std::lock_guard<Mutex>(mtx);
}

template<typename Mutex, typename Mode>
[[nodiscard]] inline std::lock_guard<Mutex> make_lock_guard(Mutex& mtx, Mode mode) {
  return std::lock_guard<Mutex>(mtx, mode);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief set name of a current thread
/// @returns true if a specified name if succesully set, false - otherwise
//////////////////////////////////////////////////////////////////////////////
IRESEARCH_API bool set_thread_name(const thread_name_t name) noexcept;

IRESEARCH_API bool get_thread_name(std::basic_string<std::remove_pointer_t<thread_name_t>>& name);

}

#endif // IRESEARCH_THREAD_UTILS_H
