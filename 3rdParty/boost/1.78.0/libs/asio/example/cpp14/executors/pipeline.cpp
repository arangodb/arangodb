#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/execution.hpp>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <cctype>

using boost::asio::executor_binder;
using boost::asio::get_associated_executor;
namespace execution = boost::asio::execution;

// An executor that launches a new thread for each function submitted to it.
// This class satisfies the executor requirements.
class thread_executor
{
private:
  // Singleton execution context that manages threads launched by the new_thread_executor.
  class thread_bag
  {
    friend class thread_executor;

    void add_thread(std::thread&& t)
    {
      std::unique_lock<std::mutex> lock(mutex_);
      threads_.push_back(std::move(t));
    }

    thread_bag() = default;

    ~thread_bag()
    {
      for (auto& t : threads_)
        t.join();
    }

    std::mutex mutex_;
    std::vector<std::thread> threads_;
  };

public:
  static thread_bag& query(execution::context_t)
  {
    static thread_bag threads;
    return threads;
  }

  static constexpr auto query(execution::blocking_t)
  {
    return execution::blocking.never;
  }

  template <class Func>
  void execute(Func f) const
  {
    thread_bag& bag = query(execution::context);
    bag.add_thread(std::thread(std::move(f)));
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
  std::packaged_task<void()> task(
      [in, f = std::move(f)]() mutable
      {
        f(in);
      });
  std::future<void> fut = task.get_future();
  execution::execute(
      boost::asio::require(ex, execution::blocking.never),
      std::move(task));
  return fut;
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
  execution::execute(
      boost::asio::require(ex, execution::blocking.never),
      [in, out, f = std::move(f)]() mutable
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
  execution::execute(
      boost::asio::require(ex, execution::blocking.never),
      [out, f = std::move(f)]() mutable
      {
        f(out);
        out.stop();
      });

  // Launch the rest of the pipeline.
  return pipeline(next_in, std::move(t)...);
}

//------------------------------------------------------------------------------

#include <boost/asio/static_thread_pool.hpp>
#include <iostream>
#include <string>

using boost::asio::bind_executor;
using boost::asio::static_thread_pool;

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
  static_thread_pool pool(1);

  auto f = pipeline(reader, filter, bind_executor(pool.executor(), upper), writer);
  f.wait();
}
