//
// wait_for_all.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <iostream>

#ifdef BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR

int main()
{
  boost::asio::io_context ctx;

  boost::asio::posix::stream_descriptor in(ctx, ::dup(STDIN_FILENO));
  boost::asio::steady_timer timer(ctx, std::chrono::seconds(5));

  char data[1024];

  boost::asio::experimental::make_parallel_group(
      [&](auto token)
      {
        return in.async_read_some(boost::asio::buffer(data), token);
      },
      [&](auto token)
      {
        return timer.async_wait(token);
      }
    ).async_wait(
      boost::asio::experimental::wait_for_all(),
      [](
          std::array<std::size_t, 2> completion_order,
          boost::system::error_code ec1, std::size_t n1,
          boost::system::error_code ec2
      )
      {
        switch (completion_order[0])
        {
        case 0:
          {
            std::cout << "descriptor finished: " << ec1 << ", " << n1 << "\n";
          }
          break;
        case 1:
          {
            std::cout << "timer finished: " << ec2 << "\n";
          }
          break;
        }
      }
    );

  ctx.run();
}

#else // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
int main() {}
#endif // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
