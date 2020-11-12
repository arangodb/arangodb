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

#ifndef IRESEARCH_THREAD_UTILS_H
#define IRESEARCH_THREAD_UTILS_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "shared.hpp"

namespace iresearch {

inline void sleep_ms(size_t ms) {
  std::chrono::milliseconds duration(ms);
  std::this_thread::sleep_for(duration);
}

template< typename _Mutex >
inline std::unique_lock< _Mutex > make_lock(_Mutex& mtx) {
  return std::unique_lock< _Mutex >(mtx);
}

#define LOCK__(lock, line) lock ## line 
#define LOCK_EXPANDER__(lock, line) LOCK__(lock, line)
#define LOCK LOCK_EXPANDER__(__lock, __LINE__)

#define ADOPT_SCOPED_LOCK_NAMED(mtx, name) std::unique_lock<typename std::remove_reference<decltype(mtx)>::type> name(mtx, std::adopt_lock)
#define DEFER_SCOPED_LOCK_NAMED(mtx, name) std::unique_lock<typename std::remove_reference<decltype(mtx)>::type> name(mtx, std::defer_lock)
#define SCOPED_LOCK(mtx) std::lock_guard<typename std::remove_reference<decltype(mtx)>::type> LOCK(mtx)
#define SCOPED_LOCK_NAMED(mtx, name) std::unique_lock<typename std::remove_reference<decltype(mtx)>::type> name(mtx)
#define TRY_SCOPED_LOCK_NAMED(mtx, name) std::unique_lock<typename std::remove_reference<decltype(mtx)>::type> name(mtx, std::try_to_lock)

} // ROOT

#endif // IRESEARCH_THREAD_UTILS_H
