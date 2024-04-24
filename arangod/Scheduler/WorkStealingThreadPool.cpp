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
#include "WorkStealingThreadPool.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <thread>

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "Logger/LogMacros.h"
#include "Metrics/Counter.h"

namespace arangodb {

namespace {
void incCounter(metrics::Counter* cnt, uint64_t delta = 1) {
  if (cnt) {
    *cnt += delta;
  }
}
}  // namespace

struct WorkStealingThreadPool::ThreadState {
  ThreadState(std::size_t id, WorkStealingThreadPool& pool);
  ~ThreadState();

  void pushBack(std::unique_ptr<WorkItem>&& work) noexcept;
  void pushFront(std::unique_ptr<WorkItem>&& work) noexcept;
  void run() noexcept;
  void signalStop() noexcept;
  bool belongsToPool(WorkStealingThreadPool const& pool) const noexcept {
    return &this->pool == &pool;
  }

  inline static thread_local ThreadState* currentThread = nullptr;

  std::size_t const id;

 private:
  void runWork(WorkItem& work) noexcept;
  auto stealWork() noexcept -> std::unique_ptr<WorkItem>;
  auto pushMany(WorkItem* item) -> std::unique_ptr<WorkItem>;
  void goToSleep(std::unique_lock<std::mutex>& guard) noexcept;

  WorkStealingThreadPool& pool;

  std::atomic<WorkItem*> pushQueue{nullptr};
  std::atomic<bool> sleeping = false;

  alignas(64) std::atomic<bool> stop{false};
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::unique_ptr<WorkItem>> _queue;
  std::mt19937 rng;
  ThreadState* prevHint = nullptr;
};

WorkStealingThreadPool::ThreadState::ThreadState(std::size_t id,
                                                 WorkStealingThreadPool& pool)
    : id(id), pool(pool), rng(id) {}

void WorkStealingThreadPool::ThreadState::pushBack(
    std::unique_ptr<WorkItem>&& work) noexcept {
  TRI_ASSERT(work != nullptr);
  TRI_ASSERT(work->next == nullptr);
  auto p = work.release();
  p->next = pushQueue.load(std::memory_order_acquire);
  while (
      !pushQueue.compare_exchange_weak(p->next, p, std::memory_order_seq_cst))
    ;
  // the CAS and the load both need to be seq_cst to ensure proper ordering
  if (sleeping.load(std::memory_order_seq_cst)) {
    mutex.lock();
    mutex.unlock();
    cv.notify_one();
  }
}

WorkStealingThreadPool::ThreadState::~ThreadState() {
  auto* item = pushQueue.load(std::memory_order_relaxed);
  while (item) {
    auto next = item->next;
    item->next = nullptr;  // to avoid triggering the assert in the destructor
    delete item;
    item = next;
  }
}

void WorkStealingThreadPool::ThreadState::pushFront(
    std::unique_ptr<WorkItem>&& work) noexcept {
  TRI_ASSERT(work != nullptr);
  TRI_ASSERT(work->next == nullptr);
  std::unique_lock guard(mutex);
  _queue.push_front(std::move(work));
}

void WorkStealingThreadPool::ThreadState::signalStop() noexcept {
  stop.store(true);
  mutex.lock();
  mutex.unlock();
  cv.notify_one();
}

void WorkStealingThreadPool::ThreadState::run() noexcept {
  currentThread = this;
  pool._latch.arrive_and_wait();

  unsigned stealAttempts = 0;
  unsigned maxStealAttempts =
      std::clamp<unsigned>(pool.numThreads * 200, 1000, 2000);
  while (!stop.load(std::memory_order_relaxed)) {
    std::unique_lock guard(mutex);
    if (!_queue.empty()) {
      stealAttempts = 0;
      auto item = std::move(_queue.front());
      TRI_ASSERT(item != nullptr);
      _queue.pop_front();
      guard.unlock();

      if (pool.hint.load(std::memory_order_relaxed) == this) {
        pool.hint.store(prevHint, std::memory_order_relaxed);
      }

      runWork(*item);
    } else if (pushQueue.load(std::memory_order_relaxed) != nullptr) {
      auto item = pushQueue.exchange(nullptr, std::memory_order_acquire);
      if (item != nullptr) {
        auto work = pushMany(item);
        guard.unlock();
        runWork(*work);
      }
    } else if (stealAttempts > maxStealAttempts) {
      // nothing to work -> go to sleep
      goToSleep(guard);
      stealAttempts = 0;
    } else {
      guard.unlock();
      if (stealAttempts == 0) {
        prevHint = pool.hint.load(std::memory_order_relaxed);
        pool.hint.store(this, std::memory_order_relaxed);
      }
      auto work = stealWork();
      if (work) {
        runWork(*work);
      } else {
        ++stealAttempts;
      }
    }
  }
}

void WorkStealingThreadPool::ThreadState::goToSleep(
    std::unique_lock<std::mutex>& guard) noexcept {
  sleeping.store(true, std::memory_order_seq_cst);
  // the store and load both need to be seq_cst to ensure proper ordering with
  // the push that wakes us up
  if (pushQueue.load(std::memory_order_seq_cst) == nullptr) {
    cv.wait_for(guard, std::chrono::milliseconds{100});
  }
  sleeping.store(false, std::memory_order_relaxed);
}

auto WorkStealingThreadPool::ThreadState::pushMany(WorkItem* item)
    -> std::unique_ptr<WorkItem> {
  TRI_ASSERT(item != nullptr);
  while (item->next != nullptr) {
    auto next = item->next;
    item->next = nullptr;
    _queue.push_front(std::unique_ptr<WorkItem>(item));
    item = next;
    TRI_ASSERT(item != nullptr);
  }
  TRI_ASSERT(item->next == nullptr);
  return std::unique_ptr<WorkItem>{item};
}

void WorkStealingThreadPool::ThreadState::runWork(WorkItem& work) noexcept {
  incCounter(pool._metrics.jobsDequeued);
  try {
    work.invoke();
  } catch (...) {
    LOG_TOPIC("d5fb3", WARN, Logger::FIXME)
        << "Scheduler just swallowed an exception.";
  }
  incCounter(pool._metrics.jobsDone);
  pool.statistics.done.fetch_add(1);
}

auto WorkStealingThreadPool::ThreadState::stealWork() noexcept
    -> std::unique_ptr<WorkItem> {
  auto idx = rng() % pool.numThreads;
  if (idx == id) {
    return {};
  }

  auto& other = pool._threadStates[idx];
  if (other->pushQueue.load(std::memory_order_relaxed) != nullptr) {
    auto* item = other->pushQueue.exchange(nullptr, std::memory_order_acquire);
    if (item) {
      std::unique_lock guard(mutex);
      return pushMany(item);
    }
  }

  std::unique_lock guard(other->mutex);
  if (other->_queue.empty()) {
    return {};
  }

  auto work = std::move(other->_queue.back());
  TRI_ASSERT(work != nullptr);
  other->_queue.pop_back();

  return work;
}

WorkStealingThreadPool::WorkStealingThreadPool(const char* name,
                                               std::size_t threadCount,
                                               ThreadPoolMetrics metrics)
    : numThreads(threadCount),
      _metrics(metrics),
      _threads(),
      _threadStates(threadCount),
      _latch(threadCount) {
  for (std::size_t i = 0; i < threadCount; ++i) {
    _threads.emplace_back(std::jthread([this, i] {
      // we intentionally create the ThreadStates _inside_ the thread
      _threadStates[i] = std::make_unique<ThreadState>(i, *this);
      _threadStates[i]->run();
    }));

#ifdef TRI_HAVE_SYS_PRCTL_H
    pthread_setname_np(_threads.back().native_handle(), name);
#endif
  }
  // wait until all threads are initialized
  _latch.wait();
}

WorkStealingThreadPool::~WorkStealingThreadPool() { shutdown(); }

void WorkStealingThreadPool::shutdown() noexcept {
  for (auto& thread : _threadStates) {
    thread->signalStop();
  }

  for (auto& thread : _threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void WorkStealingThreadPool::push(std::unique_ptr<WorkItem>&& task) noexcept {
  auto* ts = ThreadState::currentThread;
  if (ts != nullptr && ts->belongsToPool(*this)) {
    ts->pushFront(std::move(task));
  } else {
    ts = hint.load(std::memory_order_relaxed);
    if (ts != nullptr) {
      ts->pushBack(std::move(task));
    } else {
      auto idx = pushIdx.fetch_add(1, std::memory_order_relaxed) % numThreads;
      _threadStates[idx]->pushBack(std::move(task));
    }
    incCounter(_metrics.jobsQueued);
    statistics.queued.fetch_add(1);
  }
}

}  // namespace arangodb
