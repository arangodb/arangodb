
#include "InstrumentedMutex.h"

#include "Basics/FutureSharedLock.h"

using namespace arangodb;

namespace arangodb {

template struct InstrumentedMutex<std::mutex>;
template struct InstrumentedMutex<std::shared_mutex>;

struct MyScheduler {
  struct WorkHandle {};
  template<typename F>
  void queue(F&&) {}
  template<typename F, typename D>
  WorkHandle queueDelayed(F&&, D) {
    return {};
  }
};

template<typename Scheduler>
struct InstrumentedMutexTraits<futures::FutureSharedLock<Scheduler>> {
  template<typename F>
  auto lock_exclusive(futures::FutureSharedLock<Scheduler>& m, F&& fn) {
    return m.asyncLockExclusive().thenValue(
        [fn = std::forward<F>(fn)](auto guard) mutable {
          return std::forward<F>(fn)(std::move(guard));
        });
  }

  template<typename F>
  auto lock_shared(futures::FutureSharedLock<Scheduler>& m, F&& fn) {
    return m.asyncLockShared().thenValue(
        [fn = std::forward<F>(fn)](auto guard) mutable {
          return std::forward<F>(fn)(std::move(guard));
        });
  }

  template<typename F, typename Duration>
  auto try_lock_exclusive_for(futures::FutureSharedLock<Scheduler>& m,
                              Duration d, F&& fn) {
    return m
        .asyncTryLockExclusiveFor(
            std::chrono::duration_cast<std::chrono::milliseconds>(d))
        .thenValue([fn = std::forward<F>(fn)](auto guard) mutable {
          return std::forward<F>(fn)(std::move(guard));
        });
  }

  template<typename F, typename Duration>
  auto try_lock_shared_for(futures::FutureSharedLock<Scheduler>& m, Duration d,
                           F&& fn) {
    return m
        .asyncTryLockSharedFor(
            std::chrono::duration_cast<std::chrono::milliseconds>(d))
        .thenValue([fn = std::forward<F>(fn)](auto guard) mutable {
          return std::forward<F>(fn)(std::move(guard));
        });
  }
};

template<typename Scheduler>
struct InstrumentedMutexLockGuardTraits<
    futures::FutureSharedLock<Scheduler>,
    typename futures::FutureSharedLock<Scheduler>::LockGuard> {
  using LockGuard = futures::FutureSharedLock<Scheduler>::LockGuard;
  void unlock_exclusive(LockGuard&& guard) { guard.unlock(); }

  void unlock_shared(LockGuard&& guard) { guard.unlock(); }

  bool owns_lock(LockGuard& guard) { return guard.isLocked(); }
};

template struct InstrumentedMutex<futures::FutureSharedLock<MyScheduler>>;

void test(InstrumentedMutex<futures::FutureSharedLock<MyScheduler>>& im) {
  auto guard = im.try_lock_exclusive_for(std::chrono::seconds{10}).get();

  guard.unlock();

  auto guard2 = im.try_lock_shared_for(std::chrono::seconds{10}).get();

  guard2.unlock();
}

void test2(InstrumentedMutex<std::mutex>& im) {
  auto guard = im.try_lock_exclusive();

  guard.unlock();

  // auto guard2 = im.try_lock_shared_for(std::chrono::seconds{10}).get();
  //
  // guard2.unlock();
}

}  // namespace arangodb
