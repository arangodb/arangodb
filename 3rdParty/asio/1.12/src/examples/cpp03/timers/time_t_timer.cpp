//
// time_t_timer.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <asio.hpp>
#include <ctime>
#include <iostream>

struct time_t_traits
{
  // The time type.
  typedef std::time_t time_type;

  // The duration type.
  struct duration_type
  {
    duration_type() : value(0) {}
    duration_type(std::time_t v) : value(v) {}
    std::time_t value;
  };

  // Get the current time.
  static time_type now()
  {
    return std::time(0);
  }

  // Add a duration to a time.
  static time_type add(const time_type& t, const duration_type& d)
  {
    return t + d.value;
  }

  // Subtract one time from another.
  static duration_type subtract(const time_type& t1, const time_type& t2)
  {
    return duration_type(t1 - t2);
  }

  // Test whether one time is less than another.
  static bool less_than(const time_type& t1, const time_type& t2)
  {
    return t1 < t2;
  }

  // Convert to POSIX duration type.
  static boost::posix_time::time_duration to_posix_duration(
      const duration_type& d)
  {
    return boost::posix_time::seconds(d.value);
  }
};

typedef asio::basic_deadline_timer<
    std::time_t, time_t_traits> time_t_timer;

void handle_timeout(const asio::error_code&)
{
  std::cout << "handle_timeout\n";
}

int main()
{
  try
  {
    asio::io_context io_context;

    time_t_timer timer(io_context);

    timer.expires_from_now(5);
    std::cout << "Starting synchronous wait\n";
    timer.wait();
    std::cout << "Finished synchronous wait\n";

    timer.expires_from_now(5);
    std::cout << "Starting asynchronous wait\n";
    timer.async_wait(&handle_timeout);
    io_context.run();
    std::cout << "Finished asynchronous wait\n";
  }
  catch (std::exception& e)
  {
    std::cout << "Exception: " << e.what() << "\n";
  }

  return 0;
}
