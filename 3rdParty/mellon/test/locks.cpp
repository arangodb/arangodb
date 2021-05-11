#include "test-helper.h"

#include <mellon/locks.h>
#include <thread>

TEST(locks, simple_test) {
  mellon::future_mutex<default_test_tag> mutex;
  bool executed = false;

  // this should execute immediately
  mutex.async_lock().finally(
      [&](std::unique_lock<mellon::future_mutex<default_test_tag>>&& lock) noexcept {
        executed = true;
        ASSERT_TRUE(mutex.is_locked());
      });

  ASSERT_EQ(mutex.queue_length(), 0);
  ASSERT_FALSE(mutex.is_locked());
  ASSERT_TRUE(executed);
}

TEST(locks, async_lock_test) {
  mellon::future_mutex<default_test_tag> mutex;
  mutex.lock();

  ASSERT_TRUE(mutex.is_locked());

  std::atomic<bool> executed = false;
  // this should not execute immediately
  auto f = mutex.async_lock().and_then(
      [&](std::unique_lock<mellon::future_mutex<default_test_tag>>&& lock) noexcept -> std::monostate {
        executed = true;
        EXPECT_TRUE(mutex.is_locked());
        return std::monostate();
      }).finalize();

  ASSERT_FALSE(executed);
  mutex.unlock();  // now the future should execute by the executor

  // this should eventually be resolved
  std::move(f).await();
  ASSERT_TRUE(executed);
  ASSERT_FALSE(mutex.is_locked());
}


struct async_executor {
  template<typename F>
  void operator()(F&& f) const noexcept {
    std::thread(std::forward<F>(f)).detach();
  }
};


TEST(locks, async_lock_test2) {
  using mutex_type = mellon::future_mutex<default_test_tag, async_executor>;
  mutex_type mutex;
  mutex.lock();

  ASSERT_TRUE(mutex.is_locked());

  std::atomic<int> executed = 0;
  // this should not execute immediately
  auto f = mutex.async_lock().and_then(
      [&](std::unique_lock<mutex_type>&& lock) noexcept -> std::monostate {
        EXPECT_TRUE(mutex.is_locked());
        EXPECT_EQ(executed, 0);
        executed = 1;
        return std::monostate();
      }).finalize();

  auto f2 = mutex.async_lock().and_then(
      [&](std::unique_lock<mutex_type>&& lock) noexcept -> std::monostate {
        EXPECT_TRUE(mutex.is_locked());
        EXPECT_EQ(executed, 1);
        executed = 2;
        return std::monostate();
      }).finalize();

  ASSERT_FALSE(executed);
  mutex.unlock();  // now the future should execute by the executor

  // this should eventually be resolved
  std::move(f).await();
  //std::cout << "awaited 1" << std::endl;
  std::move(f2).await();
  //std::cout << "awaited 2" << std::endl;
  ASSERT_EQ(executed, 2);
  ASSERT_FALSE(mutex.is_locked());
}
