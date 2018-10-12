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
#include <boost/function.hpp>
#include <iostream>
#include <queue>

using boost::asio::ip::tcp;

class handler_priority_queue : boost::asio::execution_context
{
public:
  void add(int priority, boost::function<void()> function)
  {
    handlers_.push(queued_handler(priority, function));
  }

  void execute_all()
  {
    while (!handlers_.empty())
    {
      queued_handler handler = handlers_.top();
      handler.execute();
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

    handler_priority_queue& context() const
    {
      return context_;
    }

    template <typename Function, typename Allocator>
    void dispatch(const Function& f, const Allocator&) const
    {
      context_.add(priority_, f);
    }

    template <typename Function, typename Allocator>
    void post(const Function& f, const Allocator&) const
    {
      context_.add(priority_, f);
    }

    template <typename Function, typename Allocator>
    void defer(const Function& f, const Allocator&) const
    {
      context_.add(priority_, f);
    }

    void on_work_started() const {}
    void on_work_finished() const {}

    bool operator==(const executor& other) const
    {
      return &context_ == &other.context_ && priority_ == other.priority_;
    }

    bool operator!=(const executor& other) const
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
    return boost::asio::bind_executor(executor(*this, priority), handler);
  }

private:
  class queued_handler
  {
  public:
    queued_handler(int p, boost::function<void()> f)
      : priority_(p), function_(f)
    {
    }

    void execute()
    {
      function_();
    }

    friend bool operator<(const queued_handler& a,
        const queued_handler& b)
    {
      return a.priority_ < b.priority_;
    }

  private:
    int priority_;
    boost::function<void()> function_;
  };

  std::priority_queue<queued_handler> handlers_;
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

void low_priority_handler()
{
  std::cout << "Low priority handler\n";
}

int main()
{
  boost::asio::io_context io_context;

  handler_priority_queue pri_queue;

  // Post a completion handler to be run immediately.
  boost::asio::post(io_context, pri_queue.wrap(0, low_priority_handler));

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
  timer.expires_at(boost::asio::steady_timer::time_point::min());
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
