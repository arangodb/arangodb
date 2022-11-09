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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include <vector>

namespace arangodb {

// A thread guard is a wrapper around a std::vector<std::thread> that
// facilitates joining all joinable threads on destruction or when
// the member function joinAll is called.
struct ThreadGuard {
  ThreadGuard() = default;
  explicit ThreadGuard(size_t reserve) { threads.reserve(reserve); }
  ~ThreadGuard() {
    // Note that this might throw, and then we're in trouble;
    // We accept this risk here, because if joining a thread
    // inside the destructor of ThreadGuard fails, we're in a
    // very bad place, regardless.
    joinAll();
  }

  template<typename Function, typename... Args>
  auto emplace(Function&& f, Args&&... args)
      -> std::vector<std::thread>::reference {
    return threads.emplace_back(std::forward<Function>(f),
                                std::forward<Args>(args)...);
  }

  void joinAll() {
    for (auto& t : threads) {
      if (t.joinable()) {
        t.join();
      }
    }
    threads.clear();
  }

  [[nodiscard]] auto size() const -> size_t { return threads.size(); }

  std::vector<std::thread> threads;
};

}  // namespace arangodb
