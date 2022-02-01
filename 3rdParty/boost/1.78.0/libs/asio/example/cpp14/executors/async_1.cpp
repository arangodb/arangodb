#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/static_thread_pool.hpp>
#include <iostream>
#include <string>

using boost::asio::bind_executor;
using boost::asio::get_associated_executor;
using boost::asio::static_thread_pool;
namespace execution = boost::asio::execution;

// A function to asynchronously read a single line from an input stream.
template <class IoExecutor, class Handler>
void async_getline(IoExecutor io_ex, std::istream& is, Handler handler)
{
  // Track work for the handler's associated executor.
  auto work_ex = boost::asio::prefer(
      get_associated_executor(handler, io_ex),
      execution::outstanding_work.tracked);

  // Post a function object to do the work asynchronously.
  execution::execute(
      boost::asio::require(io_ex, execution::blocking.never),
      [&is, work_ex, handler=std::move(handler)]() mutable
      {
        std::string line;
        std::getline(is, line);

        // Pass the result to the handler, via the associated executor.
        execution::execute(
            boost::asio::prefer(work_ex, execution::blocking.possibly),
            [line=std::move(line), handler=std::move(handler)]() mutable
            {
              handler(std::move(line));
            });
      });
}

int main()
{
  static_thread_pool io_pool(1);
  static_thread_pool completion_pool(1);

  std::cout << "Enter a line: ";

  async_getline(io_pool.executor(), std::cin,
      bind_executor(completion_pool.executor(),
        [](std::string line)
        {
          std::cout << "Line: " << line << "\n";
        }));

  io_pool.wait();
  completion_pool.wait();
}
