////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Async/SuspensionCounter.h"

using namespace arangodb;

bool SuspensionCounter::notify() {
  auto counter = _counter.load(std::memory_order_relaxed);
  do {
    if (counter == -1) {
      // A suspended coroutine is waiting. (Try to) set the notify counter to
      // one and resume the coroutine.

      // (1) This acquire-CAS synchronizes-with the release-CAS (2)
      // This ensures that _coroHandle is visible.
      if (_counter.compare_exchange_weak(counter, 1, std::memory_order_acquire,
                                         std::memory_order_relaxed)) {
        // Note that this can throw, in principle, if unhandled_exception()
        // throws.
        _coroHandle.resume();
        return true;
      }
      // CAS failed, counter was modified by another thread, retry
    } else {
      // No coroutine is waiting, just increase the counter.
      if (_counter.compare_exchange_weak(counter, counter + 1,
                                         std::memory_order_relaxed)) {
        return false;
      }
      // CAS failed, counter was modified by another thread, retry
    }
  } while (true);
}

bool SuspensionCounter::Awaitable::await_ready() const noexcept {
  auto counter =
      _suspensionCounter->_counter.load(std::memory_order_relaxed) > 0;
  TRI_ASSERT(counter >= 0)
      << "SuspensionCounter::await() called in a suspended state: This means "
         "await() is called while a previously acquired Awaitable still "
         "exists (and is suspended).";
  return counter;
}

std::int64_t SuspensionCounter::Awaitable::await_resume() const noexcept {
  return _suspensionCounter->_counter.exchange(0, std::memory_order_relaxed);
}

bool SuspensionCounter::Awaitable::await_suspend(
    std::coroutine_handle<> c) noexcept {
  _suspensionCounter->_coroHandle = c;
  auto counter = std::int64_t{0};
  // Try to transition from 0 to -1 (unsignaled to suspended). If it
  // fails, the coroutine will be resumed because we have been
  // notified since await_ready() was called, and the coroutine will not
  // be suspended.

  // _suspensionCounter->_coroHandle needs to be visible when
  // `_coroHandle.resume()` is called, therefore:
  // (2) This release-CAS synchronizes-with the acquire-CAS (1)
  return _suspensionCounter->_counter.compare_exchange_strong(
      counter, -1, std::memory_order_release, std::memory_order_relaxed);
}

auto SuspensionCounter::await() -> SuspensionCounter::Awaitable {
  return Awaitable{this};
}
