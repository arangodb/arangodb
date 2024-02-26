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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Metrics/Gauge.h"
#include "utils/async_utils.hpp"

namespace arangodb::iresearch {

struct IResearchExecutionPool final : public metrics::Gauge<uint64_t> {
 public:
  using Value = uint64_t;
  using metrics::Gauge<uint64_t>::Gauge;

  void setLimit(int newLimit) noexcept {
    // should not be called during execution of queries!
    TRI_ASSERT(load() == 0);
    _pool.start(newLimit, IR_NATIVE_STRING("ARS-2"));
    _limit = newLimit;
  }

  void stop() {
    TRI_ASSERT(load() == 0);
    _pool.stop(true);
  }

  uint64_t allocateThreads(uint64_t active, uint64_t demand);

  void releaseThreads(uint64_t active, uint64_t demand);

  using Pool = irs::async_utils::ThreadPool<false>;

  bool run(Pool::Func&& fn) { return _pool.run(std::move(fn)); }

 private:
  Pool _pool;
  std::atomic_uint64_t _active{0};
  uint64_t _limit{0};
};
}  // namespace arangodb::iresearch
