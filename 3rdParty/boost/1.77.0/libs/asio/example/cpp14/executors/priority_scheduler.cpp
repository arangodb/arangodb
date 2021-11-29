#include <boost/asio/execution.hpp>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>

namespace execution = boost::asio::execution;

namespace custom_props {

  struct priority
  {
    template <typename T>
    static constexpr bool is_applicable_property_v =
      execution::is_executor<T>::value;

    static constexpr bool is_requirable = true;
    static constexpr bool is_preferable = true;

    using polymorphic_query_result_type = int;

    int value() const { return value_; }

    int value_ = 1;
  };

  constexpr priority low_priority{0};
  constexpr priority normal_priority{1};
  constexpr priority high_priority{2};

} // namespace custom_props

class priority_scheduler
{
public:
  // A class that satisfies the Executor requirements.
  class executor_type
  {
  public:
    executor_type(priority_scheduler& ctx) noexcept
      : context_(ctx), priority_(custom_props::normal_priority.value())
    {
    }

    priority_scheduler& query(execution::context_t) const noexcept
    {
      return context_;
    }

    int query(custom_props::priority) const noexcept
    {
      return priority_;
    }

    executor_type require(custom_props::priority pri) const
    {
      executor_type new_ex(*this);
      new_ex.priority_ = pri.value();
      return new_ex;
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

  executor_type executor() noexcept
  {
    return executor_type(*const_cast<priority_scheduler*>(this));
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
  auto ex = sched.executor();
  auto prefer_low = boost::asio::prefer(ex, custom_props::low_priority);
  auto low = boost::asio::require(ex, custom_props::low_priority);
  auto med = boost::asio::require(ex, custom_props::normal_priority);
  auto high = boost::asio::require(ex, custom_props::high_priority);
  execution::any_executor<custom_props::priority> poly_high(high);
  execution::execute(prefer_low, []{ std::cout << "1\n"; });
  execution::execute(low, []{ std::cout << "11\n"; });
  execution::execute(low, []{ std::cout << "111\n"; });
  execution::execute(med, []{ std::cout << "2\n"; });
  execution::execute(med, []{ std::cout << "22\n"; });
  execution::execute(high, []{ std::cout << "3\n"; });
  execution::execute(high, []{ std::cout << "33\n"; });
  execution::execute(high, []{ std::cout << "333\n"; });
  execution::execute(poly_high, []{ std::cout << "3333\n"; });
  execution::execute(boost::asio::require(ex, custom_props::priority{-1}), [&]{ sched.stop(); });
  sched.run();
  std::cout << "polymorphic query result = " << boost::asio::query(poly_high, custom_props::priority{}) << "\n";
}
