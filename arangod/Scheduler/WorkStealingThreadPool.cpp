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
#include <memory>
#include <mutex>
#include <thread>

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "Basics/cpu-relax.h"
#include "Logger/LogMacros.h"

#include "WorkStealingThreadPool.h"

namespace arangodb {

namespace {
struct StopWorkItem : WorkStealingThreadPool::WorkItem {
  void invoke() noexcept override {}
} stopItem;

}  // namespace

struct WorkStealingThreadPool::Thread {
  Thread(std::size_t id, WorkStealingThreadPool& pool);

  void push(std::unique_ptr<WorkItem>&& work) noexcept;
  void run() noexcept;

  void stealWork() noexcept;

  std::size_t const id;
  WorkStealingThreadPool& pool;
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::unique_ptr<WorkItem>> _queue;
  std::jthread _thread;

  std::mt19937 rng;

  inline static thread_local Thread* currentThread = nullptr;
};

WorkStealingThreadPool::Thread::Thread(std::size_t id,
                                       WorkStealingThreadPool& pool)
    : id(id), pool(pool), _thread(&Thread::run, this), rng(id) {}

void WorkStealingThreadPool::Thread::push(
    std::unique_ptr<WorkItem>&& work) noexcept {
  std::unique_lock guard(mutex);
  _queue.push_back(std::move(work));
}

void WorkStealingThreadPool::Thread::run() noexcept {
  currentThread = this;
  pool._latch.arrive_and_wait();

  while (true) {
    std::unique_lock guard(mutex);
    if (!_queue.empty()) {
      auto item = std::move(_queue.front());
      _queue.pop_front();
      guard.unlock();

      if (item.get() == &stopItem) {
        std::ignore = item.release();
        return;
      }

      try {
        item->invoke();
      } catch (...) {
        LOG_TOPIC("d5fb2", WARN, Logger::FIXME)
            << "Scheduler just swallowed an exception.";
      }
      pool.statistics.done += 1;
    } else {
      guard.unlock();
      stealWork();
    }
  }
}

void WorkStealingThreadPool::Thread::stealWork() noexcept {
  auto idx = rng() % pool.numThreads;
  if (idx == id) {
    return;
  }

  auto& other = pool._threads[idx];
  other->mutex.lock();
  if (other->_queue.empty()) {
    other->mutex.unlock();
    return;
  }

  if (other->_queue.back().get() == &stopItem) {
    other->mutex.unlock();
    return;
  }

  auto work = std::move(other->_queue.back());
  other->_queue.pop_back();
  other->mutex.unlock();

  try {
    work->invoke();
  } catch (...) {
    LOG_TOPIC("d5fb2", WARN, Logger::FIXME)
        << "Scheduler just swallowed an exception.";
  }
}

WorkStealingThreadPool::WorkStealingThreadPool(const char* name,
                                               std::size_t threadCount)
    : numThreads(threadCount), _threads(), _latch(threadCount) {
  for (std::size_t i = 0; i < threadCount; ++i) {
    _threads.emplace_back(std::make_unique<Thread>(i, *this));
#ifdef TRI_HAVE_SYS_PRCTL_H
    pthread_setname_np(_threads.back()->_thread.native_handle(), name);
#endif
  }
}

WorkStealingThreadPool::~WorkStealingThreadPool() {
  // push as many stop items as we have threads
  for ([[maybe_unused]] auto& thread : _threads) {
    thread->push(std::unique_ptr<WorkItem>(&stopItem));
  }

  for (auto& thread : _threads) {
    thread->_thread.join();
  }
}

void WorkStealingThreadPool::push(std::unique_ptr<WorkItem>&& task) noexcept {
  if (Thread::currentThread != nullptr &&
      &Thread::currentThread->pool == this) {
    Thread::currentThread->push(std::move(task));
  } else {
    auto idx = pushIdx.fetch_add(1, std::memory_order_relaxed) % numThreads;
    _threads[idx]->push(std::move(task));
  }
  statistics.queued.fetch_add(1);
}

}  // namespace arangodb
