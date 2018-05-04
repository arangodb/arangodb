#include <asio/associated_executor.hpp>
#include <asio/bind_executor.hpp>
#include <asio/execution_context.hpp>
#include <asio/post.hpp>
#include <asio/system_executor.hpp>
#include <asio/use_future.hpp>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using asio::execution_context;
using asio::executor_binder;
using asio::get_associated_executor;
using asio::post;
using asio::system_executor;
using asio::use_future;
using asio::use_service;

// An executor that launches a new thread for each function submitted to it.
// This class satisfies the Executor requirements.
class thread_executor
{
private:
  // Service to track all threads started through a thread_executor.
  class thread_bag : public execution_context::service
  {
  public:
    typedef thread_bag key_type;

    explicit thread_bag(execution_context& ctx)
      : execution_context::service(ctx)
    {
    }

    void add_thread(std::thread&& t)
    {
      std::unique_lock<std::mutex> lock(mutex_);
      threads_.push_back(std::move(t));
    }

  private:
    virtual void shutdown()
    {
      for (auto& t : threads_)
        t.join();
    }

    std::mutex mutex_;
    std::vector<std::thread> threads_;
  };

public:
  execution_context& context() const noexcept
  {
    return system_executor().context();
  }

  void on_work_started() const noexcept
  {
    // This executor doesn't count work.
  }

  void on_work_finished() const noexcept
  {
    // This executor doesn't count work.
  }

  template <class Func, class Alloc>
  void dispatch(Func&& f, const Alloc& a) const
  {
    post(std::forward<Func>(f), a);
  }

  template <class Func, class Alloc>
  void post(Func f, const Alloc&) const
  {
    thread_bag& bag = use_service<thread_bag>(context());
    bag.add_thread(std::thread(std::move(f)));
  }

  template <class Func, class Alloc>
  void defer(Func&& f, const Alloc& a) const
  {
    post(std::forward<Func>(f), a);
  }

  friend bool operator==(const thread_executor&,
      const thread_executor&) noexcept
  {
    return true;
  }

  friend bool operator!=(const thread_executor&,
      const thread_executor&) noexcept
  {
    return false;
  }
};

// Base class for all thread-safe queue implementations.
class queue_impl_base
{
  template <class> friend class queue_front;
  template <class> friend class queue_back;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool stop_ = false;
};

// Underlying implementation of a thread-safe queue, shared between the
// queue_front and queue_back classes.
template <class T>
class queue_impl : public queue_impl_base
{
  template <class> friend class queue_front;
  template <class> friend class queue_back;
  std::queue<T> queue_;
};

// The front end of a queue between consecutive pipeline stages.
template <class T>
class queue_front
{
public:
  typedef T value_type;

  explicit queue_front(std::shared_ptr<queue_impl<T>> impl)
    : impl_(impl)
  {
  }

  void push(T t)
  {
    std::unique_lock<std::mutex> lock(impl_->mutex_);
    impl_->queue_.push(std::move(t));
    impl_->condition_.notify_one();
  }

  void stop()
  {
    std::unique_lock<std::mutex> lock(impl_->mutex_);
    impl_->stop_ = true;
    impl_->condition_.notify_one();
  }

private:
  std::shared_ptr<queue_impl<T>> impl_;
};

// The back end of a queue between consecutive pipeline stages.
template <class T>
class queue_back
{
public:
  typedef T value_type;

  explicit queue_back(std::shared_ptr<queue_impl<T>> impl)
    : impl_(impl)
  {
  }

  bool pop(T& t)
  {
    std::unique_lock<std::mutex> lock(impl_->mutex_);
    while (impl_->queue_.empty() && !impl_->stop_)
      impl_->condition_.wait(lock);
    if (!impl_->queue_.empty())
    {
      t = impl_->queue_.front();
      impl_->queue_.pop();
      return true;
    }
    return false;
  }

private:
  std::shared_ptr<queue_impl<T>> impl_;
};

// Launch the last stage in a pipeline.
template <class T, class F>
std::future<void> pipeline(queue_back<T> in, F f)
{
  // Get the function's associated executor, defaulting to thread_executor.
  auto ex = get_associated_executor(f, thread_executor());

  // Run the function, and as we're the last stage return a future so that the
  // caller can wait for the pipeline to finish.
  return post(ex, use_future([in, f]() mutable { f(in); }));
}

// Launch an intermediate stage in a pipeline.
template <class T, class F, class... Tail>
std::future<void> pipeline(queue_back<T> in, F f, Tail... t)
{
  // Determine the output queue type.
  typedef typename executor_binder<F, thread_executor>::second_argument_type::value_type output_value_type;

  // Create the output queue and its implementation.
  auto out_impl = std::make_shared<queue_impl<output_value_type>>();
  queue_front<output_value_type> out(out_impl);
  queue_back<output_value_type> next_in(out_impl);

  // Get the function's associated executor, defaulting to thread_executor.
  auto ex = get_associated_executor(f, thread_executor());

  // Run the function.
  post(ex, [in, out, f]() mutable
      {
        f(in, out);
        out.stop();
      });

  // Launch the rest of the pipeline.
  return pipeline(next_in, std::move(t)...);
}

// Launch the first stage in a pipeline.
template <class F, class... Tail>
std::future<void> pipeline(F f, Tail... t)
{
  // Determine the output queue type.
  typedef typename executor_binder<F, thread_executor>::argument_type::value_type output_value_type;

  // Create the output queue and its implementation.
  auto out_impl = std::make_shared<queue_impl<output_value_type>>();
  queue_front<output_value_type> out(out_impl);
  queue_back<output_value_type> next_in(out_impl);

  // Get the function's associated executor, defaulting to thread_executor.
  auto ex = get_associated_executor(f, thread_executor());

  // Run the function.
  post(ex, [out, f]() mutable
      {
        f(out);
        out.stop();
      });

  // Launch the rest of the pipeline.
  return pipeline(next_in, std::move(t)...);
}

//------------------------------------------------------------------------------

#include <asio/thread_pool.hpp>
#include <iostream>
#include <string>

using asio::bind_executor;
using asio::thread_pool;

void reader(queue_front<std::string> out)
{
  std::string line;
  while (std::getline(std::cin, line))
    out.push(line);
}

void filter(queue_back<std::string> in, queue_front<std::string> out)
{
  std::string line;
  while (in.pop(line))
    if (line.length() > 5)
      out.push(line);
}

void upper(queue_back<std::string> in, queue_front<std::string> out)
{
  std::string line;
  while (in.pop(line))
  {
    std::string new_line;
    for (char c : line)
      new_line.push_back(std::toupper(c));
    out.push(new_line);
  }
}

void writer(queue_back<std::string> in)
{
  std::size_t count = 0;
  std::string line;
  while (in.pop(line))
    std::cout << count++ << ": " << line << std::endl;
}

int main()
{
  thread_pool pool;

  auto f = pipeline(reader, filter, bind_executor(pool, upper), writer);
  f.wait();
}
