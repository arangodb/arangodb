//
// prioritised_handlers.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <queue>

using boost::asio::ip::tcp;

class handler_priority_queue : boost::asio::execution_context
{
public:
  template <typename Function>
  void add(int priority, Function function)
  {
    std::unique_ptr<queued_handler_base> handler(
        new queued_handler<Function>(
          priority, std::move(function)));

    handlers_.push(std::move(handler));
  }

  void execute_all()
  {
    while (!handlers_.empty())
    {
      handlers_.top()->execute();
      handlers_.pop();
    }
  }

  class executor
  {
  public:
    executor(handler_priority_queue& q, int p)
      : context_(q), priority_(p)
    {
    }

    handler_priority_queue& context() const noexcept
    {
      return context_;
    }

    template <typename Function, typename Allocator>
    void dispatch(Function f, const Allocator&) const
    {
      context_.add(priority_, std::move(f));
    }

    template <typename Function, typename Allocator>
    void post(Function f, const Allocator&) const
    {
      context_.add(priority_, std::move(f));
    }

    template <typename Function, typename Allocator>
    void defer(Function f, const Allocator&) const
    {
      context_.add(priority_, std::move(f));
    }

    void on_work_started() const noexcept {}
    void on_work_finished() const noexcept {}

    bool operator==(const executor& other) const noexcept
    {
      return &context_ == &other.context_ && priority_ == other.priority_;
    }

    bool operator!=(const executor& other) const noexcept
    {
      return !operator==(other);
    }

  private:
    handler_priority_queue& context_;
    int priority_;
  };

  template <typename Handler>
  boost::asio::executor_binder<Handler, executor>
  wrap(int priority, Handler handler)
  {
    return boost::asio::bind_executor(
        executor(*this, priority), std::move(handler));
  }

private:
  class queued_handler_base
  {
  public:
    queued_handler_base(int p)
      : priority_(p)
    {
    }

    virtual ~queued_handler_base()
    {
    }

    virtual void execute() = 0;

    friend bool operator<(const std::unique_ptr<queued_handler_base>& a,
        const std::unique_ptr<queued_handler_base>& b) noexcept
    {
      return a->priority_ < b->priority_;
    }

  private:
    int priority_;
  };

  template <typename Function>
  class queued_handler : public queued_handler_base
  {
  public:
    queued_handler(int p, Function f)
      : queued_handler_base(p), function_(std::move(f))
    {
    }

    void execute() override
    {
      function_();
    }

  private:
    Function function_;
  };

  std::priority_queue<std::unique_ptr<queued_handler_base>> handlers_;
};

//----------------------------------------------------------------------

void high_priority_handler(const boost::system::error_code& /*ec*/)
{
  std::cout << "High priority handler\n";
}

void middle_priority_handler(const boost::system::error_code& /*ec*/)
{
  std::cout << "Middle priority handler\n";
}

struct low_priority_handler
{
  // Make the handler a move-only type.
  low_priority_handler() = default;
  low_priority_handler(const low_priority_handler&) = delete;
  low_priority_handler(low_priority_handler&&) = default;

  void operator()()
  {
    std::cout << "Low priority handler\n";
  }
};

int main()
{
  boost::asio::io_context io_context;

  handler_priority_queue pri_queue;

  // Post a completion handler to be run immediately.
  boost::asio::post(io_context, pri_queue.wrap(0, low_priority_handler()));

  // Start an asynchronous accept that will complete immediately.
  tcp::endpoint endpoint(boost::asio::ip::address_v4::loopback(), 0);
  tcp::acceptor acceptor(io_context, endpoint);
  tcp::socket server_socket(io_context);
  acceptor.async_accept(server_socket,
      pri_queue.wrap(100, high_priority_handler));
  tcp::socket client_socket(io_context);
  client_socket.connect(acceptor.local_endpoint());

  // Set a deadline timer to expire immediately.
  boost::asio::steady_timer timer(io_context);
  timer.expires_at(boost::asio::steady_timer::clock_type::time_point::min());
  timer.async_wait(pri_queue.wrap(42, middle_priority_handler));

  while (io_context.run_one())
  {
    // The custom invocation hook adds the handlers to the priority queue
    // rather than executing them from within the poll_one() call.
    while (io_context.poll_one())
      ;

    pri_queue.execute_all();
  }

  return 0;
}
