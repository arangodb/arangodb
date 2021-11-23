//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2011,2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/future.hpp>

// class promise<R>

// void promise::set_value_at_thread_exit(R&& p);

#define BOOST_THREAD_VERSION 3

#include <boost/thread/future.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/thread/detail/memory.hpp>
#include <boost/thread/csbl/memory/unique_ptr.hpp>

boost::promise<boost::csbl::unique_ptr<int> > p;
boost::promise<boost::csbl::unique_ptr<int> > p2;
void func()
{
  boost::csbl::unique_ptr<int> uptr(new int(5));
  p.set_value_at_thread_exit(boost::move(uptr));
}
void func2()
{
  p2.set_value_at_thread_exit(boost::csbl::make_unique<int>(5));
}

int main()
{
  {
    boost::future<boost::csbl::unique_ptr<int> > f = p.get_future();
    boost::thread(func).detach();
    BOOST_TEST(*f.get() == 5);
  }
  {
    boost::future<boost::csbl::unique_ptr<int> > f = p2.get_future();
    boost::thread(func2).detach();
    BOOST_TEST(*f.get() == 5);
  }

  return boost::report_errors();
}

