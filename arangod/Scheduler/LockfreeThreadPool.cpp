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
#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <thread>

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "Basics/cpu-relax.h"
#include "Logger/LogMacros.h"

#include "LockfreeThreadPool.h"

namespace arangodb {

namespace {
struct StopWorkItem : LockfreeThreadPool::WorkItem {
  void invoke() noexcept override {}
} stopItem;

}  // namespace

LockfreeThreadPool::LockfreeThreadPool(const char* name,
                                       std::size_t threadCount,
                                       ThreadPoolMetrics metrics)
    : numThreads(threadCount), _threads(), _metrics(metrics) {
  unsigned cnt = 1;
  std::generate_n(std::back_inserter(_threads), threadCount, [&]() {
    auto id = cnt++;
    auto thread = std::jthread([this, id]() noexcept {
      while (true) {
        auto item = pop(id);
        if (item.get() == &stopItem) {
          std::ignore = item.release();
          return;
        }

        try {
          item->invoke();
        } catch (std::exception const& e) {
          LOG_TOPIC("d5fb3", WARN, Logger::FIXME)
              << "Scheduler just swallowed an exception: " << e.what();
        } catch (...) {
          LOG_TOPIC("d5fb4", WARN, Logger::FIXME)
              << "Scheduler just swallowed an exception.";
        }
        statistics.done += 1;
      }
    });

#ifdef TRI_HAVE_SYS_PRCTL_H
    pthread_setname_np(thread.native_handle(), name);
#endif

    return thread;
  });
}

LockfreeThreadPool::~LockfreeThreadPool() { shutdown(); }

void LockfreeThreadPool::shutdown() noexcept {
  // push as many stop items as we have threads
  for ([[maybe_unused]] auto& thread : _threads) {
    push(std::unique_ptr<WorkItem>(&stopItem));
  }
  for (auto& thread : _threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  // pop remaining items from the queue and release them
  WorkItem* item;
  while (_queue.pop(item)) {
    if (item != &stopItem) {
      delete item;
    }
  }
}

void LockfreeThreadPool::push(std::unique_ptr<WorkItem>&& task) noexcept {
  _queue.push(task.release());
  statistics.queued.fetch_add(1);
  auto old = statistics.inQueue.fetch_add(1);
  if (old < static_cast<std::int64_t>(numThreads)) {
    statistics.inQueue.notify_one();
  }
}

auto LockfreeThreadPool::pop(unsigned id) noexcept
    -> std::unique_ptr<WorkItem> {
  while (true) {
    unsigned const maxTries = 10 + 4096 * 4 / (id * id * id);
    unsigned tryCount = 0;
    while (true) {
      WorkItem* task = nullptr;
      if (_queue.pop(task)) {
        statistics.inQueue.fetch_sub(1, std::memory_order_relaxed);
        return std::unique_ptr<WorkItem>{task};
      }

      if (tryCount++ > maxTries) {
        break;
      } else {
        basics::cpu_relax();
      }
    }

    auto v = statistics.inQueue.load();
    if (v <= 0) {
      statistics.inQueue.wait(v);
    }
  }
}

}  // namespace arangodb
