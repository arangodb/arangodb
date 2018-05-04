#include <asio/ts/executor.hpp>
#include <asio/thread_pool.hpp>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

using asio::dispatch;
using asio::execution_context;
using asio::thread_pool;

// A fixed-size thread pool used to implement fork/join semantics. Functions
// are scheduled using a simple FIFO queue. Implementing work stealing, or
// using a queue based on atomic operations, are left as tasks for the reader.
class fork_join_pool : public execution_context
{
public:
  // The constructor starts a thread pool with the specified number of threads.
  // Note that the thread_count is not a fixed limit on the pool's concurrency.
  // Additional threads may temporarily be added to the pool if they join a
  // fork_executor.
  explicit fork_join_pool(
      std::size_t thread_count = std::thread::hardware_concurrency() * 2)
    : use_count_(1),
      threads_(thread_count)
  {
    try
    {
      // Ask each thread in the pool to dequeue and execute functions until
      // it is time to shut down, i.e. the use count is zero.
      for (thread_count_ = 0; thread_count_ < thread_count; ++thread_count_)
      {
        dispatch(threads_, [&]
            {
              std::unique_lock<std::mutex> lock(mutex_);
              while (use_count_ > 0)
                if (!execute_next(lock))
                  condition_.wait(lock);
            });
      }
    }
    catch (...)
    {
      stop_threads();
      threads_.join();
      throw;
    }
  }

  // The destructor waits for the pool to finish executing functions.
  ~fork_join_pool()
  {
    stop_threads();
    threads_.join();
  }

private:
  friend class fork_executor;

  // The base for all functions that are queued in the pool.
  struct function_base
  {
    std::shared_ptr<std::size_t> work_count_;
    void (*execute_)(std::shared_ptr<function_base>& p);
  };

  // Execute the next function from the queue, if any. Returns true if a
  // function was executed, and false if the queue was empty.
  bool execute_next(std::unique_lock<std::mutex>& lock)
  {
    if (queue_.empty())
      return false;
    auto p(queue_.front());
    queue_.pop();
    lock.unlock();
    execute(lock, p); 
    return true;
  }

  // Execute a function and decrement the outstanding work.
  void execute(std::unique_lock<std::mutex>& lock,
      std::shared_ptr<function_base>& p)
  {
    std::shared_ptr<std::size_t> work_count(std::move(p->work_count_));
    try
    {
      p->execute_(p);
      lock.lock();
      do_work_finished(work_count);
    }
    catch (...)
    {
      lock.lock();
      do_work_finished(work_count);
      throw;
    }
  }

  // Increment outstanding work.
  void do_work_started(const std::shared_ptr<std::size_t>& work_count) noexcept
  {
    if (++(*work_count) == 1)
      ++use_count_;
  }

  // Decrement outstanding work. Notify waiting threads if we run out.
  void do_work_finished(const std::shared_ptr<std::size_t>& work_count) noexcept
  {
    if (--(*work_count) == 0)
    {
      --use_count_;
      condition_.notify_all();
    }
  }

  // Dispatch a function, executing it immediately if the queue is already
  // loaded. Otherwise adds the function to the queue and wakes a thread.
  void do_dispatch(std::shared_ptr<function_base> p,
      const std::shared_ptr<std::size_t>& work_count)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.size() > thread_count_ * 16)
    {
      do_work_started(work_count);
      lock.unlock();
      execute(lock, p);
    }
    else
    {
      queue_.push(p);
      do_work_started(work_count);
      condition_.notify_one();
    }
  }

  // Add a function to the queue and wake a thread.
  void do_post(std::shared_ptr<function_base> p,
      const std::shared_ptr<std::size_t>& work_count)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(p);
    do_work_started(work_count);
    condition_.notify_one();
  }

  // Ask all threads to shut down.
  void stop_threads()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    --use_count_;
    condition_.notify_all();
  }

  std::mutex mutex_;
  std::condition_variable condition_;
  std::queue<std::shared_ptr<function_base>> queue_;
  std::size_t use_count_;
  std::size_t thread_count_;
  thread_pool threads_;
};

// A class that satisfies the Executor requirements. Every function or piece of
// work associated with a fork_executor is part of a single, joinable group.
class fork_executor
{
public:
  fork_executor(fork_join_pool& ctx)
    : context_(ctx),
      work_count_(std::make_shared<std::size_t>(0))
  {
  }

  fork_join_pool& context() const noexcept
  {
    return context_;
  }

  void on_work_started() const noexcept
  {
    std::lock_guard<std::mutex> lock(context_.mutex_);
    context_.do_work_started(work_count_);
  }

  void on_work_finished() const noexcept
  {
    std::lock_guard<std::mutex> lock(context_.mutex_);
    context_.do_work_finished(work_count_);
  }

  template <class Func, class Alloc>
  void dispatch(Func&& f, const Alloc& a) const
  {
    auto p(std::allocate_shared<function<Func>>(
          typename std::allocator_traits<Alloc>::template rebind_alloc<char>(a),
          std::move(f), work_count_));
    context_.do_dispatch(p, work_count_);
  }

  template <class Func, class Alloc>
  void post(Func f, const Alloc& a) const
  {
    auto p(std::allocate_shared<function<Func>>(
          typename std::allocator_traits<Alloc>::template rebind_alloc<char>(a),
          std::move(f), work_count_));
    context_.do_post(p, work_count_);
  }

  template <class Func, class Alloc>
  void defer(Func&& f, const Alloc& a) const
  {
    post(std::forward<Func>(f), a);
  }

  friend bool operator==(const fork_executor& a,
      const fork_executor& b) noexcept
  {
    return a.work_count_ == b.work_count_;
  }

  friend bool operator!=(const fork_executor& a,
      const fork_executor& b) noexcept
  {
    return a.work_count_ != b.work_count_;
  }

  // Block until all work associated with the executor is complete. While it is
  // waiting, the thread may be borrowed to execute functions from the queue.
  void join() const
  {
    std::unique_lock<std::mutex> lock(context_.mutex_);
    while (*work_count_ > 0)
      if (!context_.execute_next(lock))
        context_.condition_.wait(lock);
  }

private:
  template <class Func>
  struct function : fork_join_pool::function_base
  {
    explicit function(Func f, const std::shared_ptr<std::size_t>& w)
      : function_(std::move(f))
    {
      work_count_ = w;
      execute_ = [](std::shared_ptr<fork_join_pool::function_base>& p)
      {
        Func tmp(std::move(static_cast<function*>(p.get())->function_));
        p.reset();
        tmp();
      };
    }

    Func function_;
  };

  fork_join_pool& context_;
  std::shared_ptr<std::size_t> work_count_;
};

// Helper class to automatically join a fork_executor when exiting a scope.
class join_guard
{
public:
  explicit join_guard(const fork_executor& ex) : ex_(ex) {}
  join_guard(const join_guard&) = delete;
  join_guard(join_guard&&) = delete;
  ~join_guard() { ex_.join(); }

private:
  fork_executor ex_;
};

//------------------------------------------------------------------------------

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

fork_join_pool pool;

template <class Iterator>
void fork_join_sort(Iterator begin, Iterator end)
{
  std::size_t n = end - begin;
  if (n > 32768)
  {
    {
      fork_executor fork(pool);
      join_guard join(fork);
      dispatch(fork, [=]{ fork_join_sort(begin, begin + n / 2); });
      dispatch(fork, [=]{ fork_join_sort(begin + n / 2, end); });
    }
    std::inplace_merge(begin, begin + n / 2, end);
  }
  else
  {
    std::sort(begin, end);
  }
}

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: fork_join <size>\n";
    return 1;
  }

  std::vector<double> vec(std::atoll(argv[1]));
  std::iota(vec.begin(), vec.end(), 0);

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(vec.begin(), vec.end(), g);

  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

  fork_join_sort(vec.begin(), vec.end());

  std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - start;

  std::cout << "sort took ";
  std::cout << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  std::cout << " microseconds" << std::endl;
}
