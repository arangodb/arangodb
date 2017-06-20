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

#ifndef IRESEARCH_THREAD_UTILS_H
#define IRESEARCH_THREAD_UTILS_H

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <type_traits>

#include "shared.hpp"

NS_ROOT

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

NS_END // ROOT

#endif // IRESEARCH_THREAD_UTILS_H
