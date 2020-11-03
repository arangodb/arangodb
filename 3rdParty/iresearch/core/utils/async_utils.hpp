////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ASYNC_UTILS_H
#define IRESEARCH_ASYNC_UTILS_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>

#include "noncopyable.hpp"
#include "shared.hpp"

namespace iresearch {
namespace async_utils {

//////////////////////////////////////////////////////////////////////////////
/// @brief spinlock implementation for Win32 since std::mutex cannot be used
///        in calls going through dllmain()
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API busywait_mutex final {
 public:
  busywait_mutex();
  ~busywait_mutex();

  void lock();
  bool try_lock();
  void unlock();

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::atomic<std::thread::id> owner_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

//////////////////////////////////////////////////////////////////////////////
/// @brief a read-write mutex implementation
///        supports recursive read-lock acquisition
///        supports recursive write-lock acquisition
///        supports downgrading write-lock to a read lock
///        does not support upgrading a read-lock to a write-lock
///        write-locks are given acquisition preference over read-locks
/// @note the following will cause a deadlock with the current implementation:
///       read-lock(threadA) -> write-lock(threadB) -> read-lock(threadA)
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API read_write_mutex final {
 public:
  // for use with std::lock_guard/std::unique_lock for read operations
  class read_mutex {
   public:
    explicit read_mutex(read_write_mutex& mutex) noexcept
      : mutex_(mutex) {
    }
    read_mutex& operator=(read_mutex&) = delete; // because of reference
    void lock() { mutex_.lock_read(); }
    bool try_lock() { return mutex_.try_lock_read(); }
    void unlock() { mutex_.unlock(); }
   private:
    read_write_mutex& mutex_;
  }; // read_mutex

  // for use with std::lock_guard/std::unique_lock for write operations
  class write_mutex {
   public:
    explicit write_mutex(read_write_mutex& mutex) noexcept
      : mutex_(mutex) {
    }
    write_mutex& operator=(write_mutex&) = delete; // because of reference
    void lock() { mutex_.lock_write(); }
    bool owns_write() { return mutex_.owns_write(); }
    bool try_lock() { return mutex_.try_lock_write(); }
    void unlock(bool exclusive_only = false) { mutex_.unlock(exclusive_only); }
   private:
    read_write_mutex& mutex_;
  }; // write_mutex

  read_write_mutex() noexcept;
  ~read_write_mutex() noexcept;

  void lock_read();
  void lock_write();
  bool owns_write() noexcept;
  bool try_lock_read();
  bool try_lock_write();

  // The mutex must be locked by the current thread of execution, otherwise, the behavior is undefined.
  // @param exclusive_only if true then only downgrade a lock to a read-lock
  void unlock(bool exclusive_only = false);

 private:
   IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
   std::atomic<size_t> concurrent_count_;
   size_t exclusive_count_;
   std::atomic<std::thread::id> exclusive_owner_;
   VALGRIND_ONLY(std::mutex exclusive_owner_mutex_;)
   size_t exclusive_owner_recursion_count_;
   std::mutex mutex_;
   std::condition_variable reader_cond_;
   std::condition_variable writer_cond_;
   IRESEARCH_API_PRIVATE_VARIABLES_END
}; // read_write_mutex

class IRESEARCH_API thread_pool {
 public:
  explicit thread_pool(size_t max_threads = 0, size_t max_idle = 0);
  virtual ~thread_pool();
  virtual size_t max_idle();
  virtual void max_idle(size_t value);
  virtual void max_idle_delta(int delta); // change value by delta
  virtual size_t max_threads();
  virtual void max_threads(size_t value);
  virtual void max_threads_delta(int delta); // change value by delta
  virtual bool run(std::function<void()>&& fn);
  virtual void stop(bool skip_pending = false); // always a blocking call
  virtual size_t tasks_active();
  virtual size_t tasks_pending();
  virtual size_t threads();

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  size_t active_;
  std::condition_variable cond_;
  std::mutex lock_;
  size_t max_idle_;
  size_t max_threads_;
  std::vector<std::thread> pool_;
  std::queue<std::function<void()>> queue_;
  enum class State { ABORT, FINISH, RUN } state_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  void run();
}; // thread_pool

} // async_utils
} // namespace iresearch {

#endif
