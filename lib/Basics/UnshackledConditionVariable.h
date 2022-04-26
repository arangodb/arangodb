////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/UnshackledMutex.h"

#include <mutex>

namespace arangodb::basics {

class UnshackledConditionVariable {
 public:
  UnshackledConditionVariable() = default;
  UnshackledConditionVariable(UnshackledConditionVariable const&) = delete;

  void notify_one() noexcept;
  void notify_all() noexcept;

  void wait(std::unique_lock<UnshackledMutex>& lock);

  template<class Predicate>
  void wait(std::unique_lock<UnshackledMutex>& lock, Predicate stop_waiting);

  template<class Rep, class Period>
  auto wait_for(std::unique_lock<UnshackledMutex>& lock,
                std::chrono::duration<Rep, Period> const& rel_time)
      -> std::cv_status;

  template<class Rep, class Period, class Predicate>
  auto wait_for(std::unique_lock<UnshackledMutex>& lock,
                std::chrono::duration<Rep, Period> const& rel_time,
                Predicate stop_waiting) -> bool;

  template<class Clock, class Duration>
  auto wait_until(std::unique_lock<std::mutex>& lock,
                  std::chrono::time_point<Clock, Duration> const& timeout_time)
      -> std::cv_status;

  template<class Clock, class Duration, class Predicate>
  auto wait_until(std::unique_lock<std::mutex>& lock,
                  std::chrono::time_point<Clock, Duration> const& timeout_time,
                  Predicate stop_waiting) -> bool;

 private:
  std::mutex _mutex;
  std::condition_variable _cv;
};

template<class Predicate>
void UnshackledConditionVariable::wait(std::unique_lock<UnshackledMutex>& lock,
                                       Predicate stop_waiting) {
  while (!stop_waiting()) {
    wait(lock);
  }
}

template<class Rep, class Period, class Predicate>
auto UnshackledConditionVariable::wait_for(
    std::unique_lock<UnshackledMutex>& lock,
    std::chrono::duration<Rep, Period> const& rel_time, Predicate stop_waiting)
    -> bool {
  return wait_until(lock, std::chrono::steady_clock::now() + rel_time,
                    std::forward<Predicate>(stop_waiting));
}

template<class Clock, class Duration, class Predicate>
auto UnshackledConditionVariable::wait_until(
    std::unique_lock<std::mutex>& lock,
    std::chrono::time_point<Clock, Duration> const& timeout_time,
    Predicate stop_waiting) -> bool {
  while (!stop_waiting()) {
    if (wait_until(lock, timeout_time) == std::cv_status::timeout) {
      return stop_waiting();
    }
  }
  return true;
}

}  // namespace arangodb::basics
