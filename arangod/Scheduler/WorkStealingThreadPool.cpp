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
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include "Basics/cpu-relax.h"

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "Logger/LogMacros.h"
#include "Metrics/Counter.h"
#include "Metrics/Histogram.h"
#include "Metrics/LogScale.h"

namespace arangodb {

namespace {
void incCounter(metrics::Counter* cnt, uint64_t delta = 1) {
  if (cnt) {
    *cnt += delta;
  }
}

auto calculateCoprimes(std::size_t n) -> std::vector<std::size_t> {
  TRI_ASSERT(n > 0);
  if (n <= 2) {
    return {1};
  }
  if (n == 3) {
    return {1, 2};
  }

  std::vector<std::size_t> result;
  result.push_back(1);
  result.push_back(n - 1);

  for (std::size_t i = 2; i <= n / 2; ++i) {
    if (std::gcd(i, n) == 1) {
      result.push_back(i);
      result.push_back(n - i);
    }
  }
  return result;
}
}  // namespace

struct WorkStealingThreadPool::ThreadState {
  ThreadState(std::size_t id, std::size_t stepSize,
              WorkStealingThreadPool& pool);
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

  std::atomic<std::size_t> prevHint = NoHint;

 private:
  void wake();
  void runWork(WorkItem& work) noexcept;
  auto stealWork() noexcept -> std::unique_ptr<WorkItem>;
  auto pushMany(WorkItem* item) -> std::unique_ptr<WorkItem>;
  void goToSleep(std::unique_lock<std::mutex>& guard) noexcept;

  std::atomic<WorkItem*> pushQueue{nullptr};
  std::atomic<bool> sleeping = false;
  std::atomic<std::size_t> queueSize = 0;
  std::size_t const stepSize;
  WorkStealingThreadPool& pool;
  std::atomic<std::size_t> stealHint = NoHint;

  alignas(64) std::atomic<bool> stop{false};
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::unique_ptr<WorkItem>> _queue;
  std::mt19937 rng;
};

WorkStealingThreadPool::ThreadState::ThreadState(std::size_t id,
                                                 std::size_t stepSize,
                                                 WorkStealingThreadPool& pool)
    : id(id), stepSize(stepSize), pool(pool), rng(id) {}

WorkStealingThreadPool::ThreadState::~ThreadState() {
  auto* item = pushQueue.load(std::memory_order_relaxed);
  while (item) {
    auto next = item->next;
    item->next = nullptr;  // to avoid triggering the assert in the destructor
    delete item;
    item = next;
  }
}

void WorkStealingThreadPool::ThreadState::wake() {
  mutex.lock();
  mutex.unlock();
  cv.notify_one();
}

void WorkStealingThreadPool::ThreadState::pushBack(
    std::unique_ptr<WorkItem>&& work) noexcept {
  TRI_ASSERT(work != nullptr);
  TRI_ASSERT(work->next == nullptr);
  auto p = work.release();
  p->next = pushQueue.load(std::memory_order_acquire);
  bool hasWork = p->next != nullptr;
  // the compare_exchange implicitly reloads the next pointer
  while (
      !pushQueue.compare_exchange_weak(p->next, p, std::memory_order_seq_cst)) {
    hasWork = p->next != nullptr;
  }

  // the CAS and the load both need to be seq_cst to ensure proper ordering
  if (sleeping.load(std::memory_order_seq_cst)) {
    wake();
  } else if (hasWork && pool.numSleeping.load(std::memory_order_relaxed) > 0) {
    // we have at least two items in the pushQueue and this thread seems to be
    // busy, but we have at least one other thread sleeping -> try to wake one
    // up
    auto idx = pool.lastSleepIdx.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < pool.numThreads; ++i) {
      auto& ts = pool._threadStates[idx];
      if (ts->sleeping.load(std::memory_order_relaxed)) {
        // we found a sleeping thread -> set the hint and wake it up
        ts->stealHint.store(id, std::memory_order_relaxed);
        ts->wake();
        break;
      }
      idx += stepSize;
      if (idx >= pool.numThreads) {
        idx -= pool.numThreads;
      }
    }
  }
}

void WorkStealingThreadPool::ThreadState::pushFront(
    std::unique_ptr<WorkItem>&& work) noexcept {
  TRI_ASSERT(work != nullptr);
  TRI_ASSERT(work->next == nullptr);
  std::unique_lock guard(mutex);
  _queue.push_front(std::move(work));
  queueSize.store(_queue.size(), std::memory_order_relaxed);
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
  unsigned const d = id + 1;
  // threads with lower ids should spin longer before going to sleep
  unsigned const totalMaxStealAttempts = 1 + 4096 / (d * d * d);
  auto maxStealAttempts = totalMaxStealAttempts;

  auto doWork = [&](WorkItem& work) {
    if (pool.hint.load(std::memory_order_relaxed) == id) {
      auto h = prevHint.load(std::memory_order_relaxed);
      pool.hint.store(h, std::memory_order_relaxed);
    }

    runWork(work);

    stealAttempts = 0;
    maxStealAttempts = totalMaxStealAttempts;
  };

  while (!stop.load(std::memory_order_relaxed)) {
    std::unique_lock guard(mutex);
    if (!_queue.empty()) {
      auto item = std::move(_queue.front());
      TRI_ASSERT(item != nullptr);
      _queue.pop_front();
      queueSize.store(_queue.size(), std::memory_order_relaxed);
      guard.unlock();

      doWork(*item);
    } else if (pushQueue.load(std::memory_order_relaxed) != nullptr) {
      auto item = pushQueue.exchange(nullptr, std::memory_order_acquire);
      if (item != nullptr) {
        auto work = pushMany(item);
        guard.unlock();
        doWork(*work);
      }
    } else if (stealAttempts > maxStealAttempts) {
      // nothing to work -> go to sleep
      goToSleep(guard);
      if (maxStealAttempts > 2) {
        maxStealAttempts /= 2;
      }
      stealAttempts = 0;
    } else {
      guard.unlock();
      if (stealAttempts == 0) {
        // try to insert ourselves in the hint chain
        // threads with lower IDs spin longer, so we prefer to have them at the
        // front of the chain, provided they are not sleeping already.
        // We simply limit ourselves to 4 steps before giving up (note that the
        // chain can contain loops)
        auto* hint = &pool.hint;
        for (unsigned i = 0; i < 4; ++i) {
          auto h = hint->load(std::memory_order_relaxed);
          auto& ts = pool._threadStates[h];
          if (h > id || ts->sleeping.load(std::memory_order_relaxed)) {
            prevHint.store(h, std::memory_order_relaxed);
            hint->store(id, std::memory_order_relaxed);
            break;
          }
          hint = &ts->prevHint;
        }
      }
      auto work = stealWork();
      if (work) {
        doWork(*work);
      } else {
        basics::cpu_relax();
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
    pool.lastSleepIdx.store(id, std::memory_order_relaxed);
    pool.numSleeping.fetch_add(1, std::memory_order_relaxed);
    cv.wait_for(guard, std::chrono::milliseconds{100});
    pool.numSleeping.fetch_sub(1, std::memory_order_relaxed);
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
  queueSize.store(_queue.size(), std::memory_order_relaxed);
  TRI_ASSERT(item->next == nullptr);
  return std::unique_ptr<WorkItem>{item};
}

void WorkStealingThreadPool::ThreadState::runWork(WorkItem& work) noexcept {
  incCounter(pool._metrics.jobsDequeued);
  if (pool._metrics.dequeueTimes) {
    auto queueTime = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - work.enqueueTime);
    auto d =
        std::chrono::duration_cast<std::chrono::microseconds, double>(queueTime)
            .count();
    pool._metrics.dequeueTimes->count(d);
  }
  pool.statistics.dequeued.fetch_add(1, std::memory_order_relaxed);
  try {
    work.invoke();
  } catch (std::exception const& e) {
    LOG_TOPIC("71d10", WARN, Logger::FIXME)
        << "Scheduler just swallowed an exception: " << e.what();
  } catch (...) {
    LOG_TOPIC("c1380", WARN, Logger::FIXME)
        << "Scheduler just swallowed an exception.";
  }
  incCounter(pool._metrics.jobsDone);
  pool.statistics.done.fetch_add(1, std::memory_order_relaxed);
}

auto WorkStealingThreadPool::ThreadState::stealWork() noexcept
    -> std::unique_ptr<WorkItem> {
  // we start at a random index and iterate through the other threads
  // using our stepSize. Since the stepSizes are coprimes of the number of
  // threads this is guaranteed to cover all threads, but different threads will
  // likely use a different order.
  auto idx = stealHint.load(std::memory_order_relaxed);
  if (idx == NoHint) {
    idx = rng() % pool.numThreads;
  }

  for (std::size_t i = 0; i < pool.numThreads; ++i) {
    if (idx != id) {
      auto& other = pool._threadStates[idx];
      if (other->pushQueue.load(std::memory_order_relaxed) != nullptr) {
        auto* item =
            other->pushQueue.exchange(nullptr, std::memory_order_acquire);
        if (item) {
          std::unique_lock guard(mutex);
          auto res = pushMany(item);
          // remember this thread so next time we try to steal from it first
          stealHint.store(idx, std::memory_order_relaxed);
          return res;
        }
      }

      if (other->queueSize.load(std::memory_order_relaxed) > 0) {
        // stealing is only best effort - try to acquire lock, but don't bother
        // if we don't get it right away
        std::unique_lock guard(other->mutex, std::try_to_lock);
        if (guard.owns_lock() && !other->_queue.empty()) {
          auto toSteal = std::max<std::size_t>(1, other->_queue.size() / 2);

          auto work = std::move(other->_queue.back());
          TRI_ASSERT(work != nullptr);
          other->_queue.pop_back();
          if (toSteal > 1) {
            std::unique_lock guard2(mutex);
            for (std::size_t i = 1; i < toSteal; ++i) {
              _queue.push_front(std::move(work));

              TRI_ASSERT(other->_queue.size() > 0);
              work = std::move(other->_queue.back());
              TRI_ASSERT(work != nullptr);
              other->_queue.pop_back();
            }
            queueSize.store(_queue.size(), std::memory_order_relaxed);
          }
          other->queueSize.store(other->_queue.size(),
                                 std::memory_order_relaxed);
          guard.unlock();

          // remember this thread so next time we try to steal from it first
          stealHint.store(idx, std::memory_order_relaxed);
          return work;
        }
      }
    }
    if (this->pushQueue.load(std::memory_order_relaxed) != nullptr) {
      // work has been pushed into our queue -> no need to still anything
      return {};
    }
    idx += stepSize;
    if (idx >= pool.numThreads) {
      idx -= pool.numThreads;
    }
  }
  // nothing found - reset the hint so next time we start at a random index
  stealHint.store(NoHint, std::memory_order_relaxed);
  return {};
}

WorkStealingThreadPool::WorkStealingThreadPool(const char* name,
                                               std::size_t threadCount,
                                               ThreadPoolMetrics metrics)
    : numThreads(threadCount),
      _metrics(metrics),
      _threads(),
      _threadStates(threadCount),
      _latch(threadCount) {
  auto coprimes = calculateCoprimes(threadCount);
  for (std::size_t i = 0; i < threadCount; ++i) {
    auto stepSize = coprimes[i % coprimes.size()];
    _threads.emplace_back(std::jthread([this, i, stepSize]() {
      // we intentionally create the ThreadStates _inside_ the thread
      _threadStates[i] = std::make_unique<ThreadState>(i, stepSize, *this);
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
  if (auto* ts = ThreadState::currentThread;
      ts != nullptr && ts->belongsToPool(*this)) {
    ts->pushFront(std::move(task));
  } else {
    auto h = hint.load(std::memory_order_relaxed);
    if (h != NoHint) {
      auto prevHint =
          _threadStates[h]->prevHint.load(std::memory_order_relaxed);
      hint.store(prevHint, std::memory_order_relaxed);
      _threadStates[h]->pushBack(std::move(task));
    } else {
      auto idx = pushIdx.fetch_add(1, std::memory_order_relaxed) % numThreads;
      _threadStates[idx]->pushBack(std::move(task));
    }
  }
  incCounter(_metrics.jobsQueued);
  statistics.queued.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace arangodb
