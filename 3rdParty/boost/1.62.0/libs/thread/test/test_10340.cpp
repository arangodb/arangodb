// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4

#include <boost/thread/future.hpp>

struct foo
{
    foo(int i_): i(i_) {}
    int i;
};

int main()
{
  boost::promise<foo> p;
  const foo f(42);
  p.set_value(f);

  // Clearly a const future ref isn't much use, but I needed to
  // prove the problem wasn't me trying to copy a unique_future

  const boost::future<foo>& fut = boost::make_ready_future( foo(42) );
  return 0;
}

