////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include <condition_variable>
#include <mutex>

namespace arangodb::basics {

// @brief Normal Mutexes, like std::mutex or pthread_mutex_t, require to be
// unlocked by the same thread they were locked. However, with rising use of
// async programming / futures, this is not always practical any more.
// This mutex lifts this requirement and can be unlocked from another thread.
class UnshackledMutex {
 public:
  UnshackledMutex() = default;
  ~UnshackledMutex() = default;

  UnshackledMutex(UnshackledMutex const&) = delete;
  auto operator=(UnshackledMutex const&) -> UnshackledMutex& = delete;

  void lock();
  void unlock();
  bool try_lock();

 private:
  std::mutex _mutex;
  std::condition_variable _cv;
  bool _locked{false};
};

}  // namespace arangodb::basics
