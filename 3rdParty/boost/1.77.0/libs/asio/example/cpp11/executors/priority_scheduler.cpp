#include <boost/asio/dispatch.hpp>
#include <boost/asio/execution_context.hpp>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>

using boost::asio::dispatch;
using boost::asio::execution_context;
namespace execution = boost::asio::execution;

class priority_scheduler : public execution_context
{
public:
  // A class that satisfies the Executor requirements.
  class executor_type
  {
  public:
    executor_type(priority_scheduler& ctx, int pri) noexcept
      : context_(ctx), priority_(pri)
    {
    }

    priority_scheduler& query(execution::context_t) const noexcept
    {
      return context_;
    }

    template <class Func>
    void execute(Func f) const
    {
      auto p(std::make_shared<item<Func>>(priority_, std::move(f)));
      std::lock_guard<std::mutex> lock(context_.mutex_);
      context_.queue_.push(p);
      context_.condition_.notify_one();
    }

    friend bool operator==(const executor_type& a,
        const executor_type& b) noexcept
    {
      return &a.context_ == &b.context_;
    }

    friend bool operator!=(const executor_type& a,
        const executor_type& b) noexcept
    {
      return &a.context_ != &b.context_;
    }

  private:
    priority_scheduler& context_;
    int priority_;
  };

  ~priority_scheduler() noexcept
  {
    shutdown();
    destroy();
  }

  executor_type get_executor(int pri = 0) noexcept
  {
    return executor_type(*const_cast<priority_scheduler*>(this), pri);
  }

  void run()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;)
    {
      condition_.wait(lock, [&]{ return stopped_ || !queue_.empty(); });
      if (stopped_)
        return;
      auto p(queue_.top());
      queue_.pop();
      lock.unlock();
      p->execute_(p);
      lock.lock();
    }
  }

  void stop()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
    condition_.notify_all();
  }

private:
  struct item_base
  {
    int priority_;
    void (*execute_)(std::shared_ptr<item_base>&);
  };

  template <class Func>
  struct item : item_base
  {
    item(int pri, Func f) : function_(std::move(f))
    {
      priority_ = pri;
      execute_ = [](std::shared_ptr<item_base>& p)
      {
        Func tmp(std::move(static_cast<item*>(p.get())->function_));
        p.reset();
        tmp();
      };
    }

    Func function_;
  };

  struct item_comp
  {
    bool operator()(
        const std::shared_ptr<item_base>& a,
        const std::shared_ptr<item_base>& b)
    {
      return a->priority_ < b->priority_;
    }
  };

  std::mutex mutex_;
  std::condition_variable condition_;
  std::priority_queue<
    std::shared_ptr<item_base>,
    std::vector<std::shared_ptr<item_base>>,
    item_comp> queue_;
  bool stopped_ = false;
};

int main()
{
  priority_scheduler sched;
  auto low = sched.get_executor(0);
  auto med = sched.get_executor(1);
  auto high = sched.get_executor(2);
  dispatch(low, []{ std::cout << "1\n"; });
  dispatch(low, []{ std::cout << "11\n"; });
  dispatch(med, []{ std::cout << "2\n"; });
  dispatch(med, []{ std::cout << "22\n"; });
  dispatch(high, []{ std::cout << "3\n"; });
  dispatch(high, []{ std::cout << "33\n"; });
  dispatch(high, []{ std::cout << "333\n"; });
  dispatch(sched.get_executor(-1), [&]{ sched.stop(); });
  sched.run();
}
