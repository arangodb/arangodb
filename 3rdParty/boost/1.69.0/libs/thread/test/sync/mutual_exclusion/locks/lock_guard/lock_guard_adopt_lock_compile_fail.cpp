// Copyright (C) 2018 Tom Hughes
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/lock_guard.hpp>

// template <class Mutex> class lock_guard;

// lock_guard(Mutex& m_, adopt_lock_t)

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/detail/lightweight_test.hpp>

boost::mutex m;

void fail()
{
  boost::lock_guard<boost::mutex> lk(m, boost::adopt_lock);
}
