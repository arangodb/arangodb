
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

template struct InstrumentedMutex<futures::FutureSharedLock<MyScheduler>>;

void test2(InstrumentedMutex<std::mutex>& im) {
  {
    auto guard = im.lock_exclusive();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_exclusive();
    guard.unlock();
  }
}

void test2(InstrumentedMutex<std::shared_mutex>& im) {
  {
    auto guard = im.lock_exclusive();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_exclusive();
    guard.unlock();
  }
  {
    auto guard = im.lock_shared();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_shared();
    guard.unlock();
  }
}

void test2(InstrumentedMutex<std::timed_mutex>& im) {
  {
    auto guard = im.lock_exclusive();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_exclusive();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_exclusive_for(std::chrono::milliseconds{1});
    guard.unlock();
  }
}

void test2(InstrumentedMutex<std::shared_timed_mutex>& im) {
  {
    auto guard = im.lock_exclusive();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_exclusive();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_exclusive_for(std::chrono::milliseconds{1});
    guard.unlock();
  }
  {
    auto guard = im.lock_shared();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_shared();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_shared_for(std::chrono::milliseconds{1});
    guard.unlock();
  }
}

void test2(InstrumentedMutex<futures::FutureSharedLock<MyScheduler>>& im) {
  {
    auto guard = im.lock_exclusive().get();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_exclusive_for(std::chrono::milliseconds{1}).get();
    guard.unlock();
  }
  {
    auto guard = im.lock_shared().get();
    guard.unlock();
  }
  {
    auto guard = im.try_lock_shared_for(std::chrono::milliseconds{1}).get();
    guard.unlock();
  }
}

}  // namespace arangodb
