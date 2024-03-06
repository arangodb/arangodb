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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>
#include <memory>

#include "Gauge.h"
#include "GaugeCounterGuard.h"

namespace arangodb {

struct InstrumentedMutexMetrics {
  metrics::Gauge<uint64_t>* waitingExclusiveLocks;
  metrics::Gauge<uint64_t>* waitingSharedLocks;
  metrics::Gauge<uint64_t>* numSharedLocks;
  metrics::Gauge<uint64_t>* numExclusiveLocks;
};

template<typename Mutex>
concept isStdMutexLike = requires(Mutex m) {
  m.lock();
  m.unlock();
  m.try_lock();
};

template<typename Mutex>
concept isFutureMutexLike = requires(Mutex m, std::chrono::milliseconds t) {
  m.asyncLockShared();
  m.asyncLockExclusive();
  m.asyncTryLockSharedFor(t);
  m.asyncTryLockExclusiveFor(t);
};

template<typename T>
struct MutexTraits {
  static void lock(T& mutex) { mutex.lock(); }
};

template<typename Mutex>
struct InstrumentedMutex {
  template<typename... Args>
  InstrumentedMutex(std::shared_ptr<InstrumentedMutexMetrics> metrics,
                    Args&&... args)
      : _metrics(std::move(metrics)), _mutex(std::forward<Args>(args)...) {}

  InstrumentedMutex(InstrumentedMutex const&) = default;
  InstrumentedMutex(InstrumentedMutex&&) noexcept = default;
  InstrumentedMutex& operator=(InstrumentedMutex const&) = default;
  InstrumentedMutex& operator=(InstrumentedMutex&&) noexcept = default;

  void lock() requires isStdMutexLike<Mutex> {
    metrics::GaugeCounterGuard guard(*_metrics->waitingExclusiveLocks, 1ul);
    MutexTraits<Mutex>::lock(_mutex);
    _metrics->numExclusiveLocks += 1;
  }

  void unlock() requires isStdMutexLike<Mutex> {
    _mutex.unlock();
    _metrics->numExclusiveLocks -= 1;
  }

  bool try_lock() requires isStdMutexLike<Mutex> {
    metrics::GaugeCounterGuard guard(*_metrics->waitingExclusiveLocks, 1ul);
    bool result = _mutex.try_lock();
    if (result) {
      _metrics->numExclusiveLocks += 1;
    }
    return result;
  }

  auto asyncLockShared() requires isFutureMutexLike<Mutex> {
    metrics::GaugeCounterGuard guard(*_metrics->waitingSharedLocks, 1ul);
    return _mutex.asyncLockShared().thenValue(
        [guard = std::move(guard), metrics = _metrics](auto&& lock) mutable {
          guard.reset();
          metrics->numSharedLocks += 1;
          return std::move(lock);
        });
  }

  auto asyncLockExclusive() requires isFutureMutexLike<Mutex> {
    metrics::GaugeCounterGuard guard(*_metrics->waitingExclusiveLocks, 1ul);
    return _mutex.asyncLockExclusive().thenValue(
        [guard = std::move(guard), metrics = _metrics](auto&& lock) mutable {
          guard.reset();
          metrics->numExclusiveLocks += 1;
          return std::move(lock);
        });
  }

 private : std::shared_ptr<InstrumentedMutexMetrics> _metrics;
  Mutex _mutex;
};

}  // namespace arangodb
