////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Actor/IScheduler.h"

namespace arangodb::actor::test {
struct MockScheduler : IScheduler {
  auto start(size_t number_of_threads) -> void{};
  auto stop() -> void{};
  template<class Fn>
  auto isIdle(Fn idleCheck) const -> bool {
    return idleCheck();
  }

  void queue(LazyWorker&& worker) override { worker(); }
  void delay(std::chrono::seconds delay,
             fu2::unique_function<void(bool) noexcept>&& fn) override {
    fn(true);
  }
};

}  // namespace arangodb::actor::test
