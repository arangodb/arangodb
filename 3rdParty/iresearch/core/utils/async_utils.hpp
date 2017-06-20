//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_ASYNC_UTILS_H
#define IRESEARCH_ASYNC_UTILS_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>

#include "noncopyable.hpp"
#include "shared.hpp"

NS_ROOT
NS_BEGIN(async_utils)

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

class IRESEARCH_API read_write_mutex final {
 public:
  // for use with std::lock_guard/std::unique_lock for read operations
  class read_mutex: private util::noncopyable {
   public:
    read_mutex(read_write_mutex& mutex): mutex_(mutex) {}
    void lock() { mutex_.lock_read(); }
    bool try_lock() { return mutex_.try_lock_read(); }
    void unlock() { mutex_.unlock(); }
   private:
    read_write_mutex& mutex_;
  };

  // for use with std::lock_guard/std::unique_lock for write operations
  class write_mutex: private util::noncopyable {
   public:
    write_mutex(read_write_mutex& mutex): mutex_(mutex) {}
    void lock() { mutex_.lock_write(); }
    bool owns_write() { return mutex_.owns_write(); }
    bool try_lock() { return mutex_.try_lock_write(); }
    void unlock(bool exclusive_only = false) { mutex_.unlock(exclusive_only); }
   private:
    read_write_mutex& mutex_;
  };

  read_write_mutex();
  ~read_write_mutex();

  void lock_read();
  void lock_write();
  bool owns_write();
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
   size_t exclusive_owner_recursion_count_;
   std::mutex mutex_;
   std::condition_variable reader_cond_;
   std::condition_variable writer_cond_;
   IRESEARCH_API_PRIVATE_VARIABLES_END
};

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
};

NS_END
NS_END

#endif