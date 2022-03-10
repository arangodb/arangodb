//
// timer.cpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

void print(const boost::system::error_code& /*e*/,
    boost::asio::steady_timer* t, int* count)
{
  if (*count < 5)
  {
    std::cout << *count << std::endl;
    ++(*count);

    t->expires_at(t->expiry() + boost::asio::chrono::seconds(1));
    t->async_wait(boost::bind(print,
          boost::asio::placeholders::error, t, count));
  }
}

int main()
{
  boost::asio::io_context io;

  int count = 0;
  boost::asio::steady_timer t(io, boost::asio::chrono::seconds(1));
  t.async_wait(boost::bind(print,
        boost::asio::placeholders::error, &t, &count));

  io.run();

  std::cout << "Final count is " << count << std::endl;

  return 0;
}
