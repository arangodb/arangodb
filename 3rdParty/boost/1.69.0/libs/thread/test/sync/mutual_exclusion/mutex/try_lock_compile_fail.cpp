// Copyright (C) 2017 Tom Hughes
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/mutex.hpp>

// class mutex;

#include <boost/thread/mutex.hpp>
#include <boost/detail/lightweight_test.hpp>

void fail()
{
  boost::mutex m0;
  if (!m0.try_lock()) {
    m0.unlock();
  }
}
