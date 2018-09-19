//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/future.hpp>

// future<tuple<T>> when_any(T&&);

#include <boost/config.hpp>

#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif


#define BOOST_THREAD_VERSION 4

#include <boost/thread/future.hpp>
#include <boost/detail/lightweight_test.hpp>

#ifdef BOOST_MSVC
#pragma warning(disable: 4127) // conditional expression is constant
#endif

int p1()
{
  return 123;
}

int main()
{
#if defined BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
  if (0) // todo not yet implemented
  { // invalid future copy-constructible
    boost::future<int> f1;
    BOOST_TEST(! f1.valid());
    boost::future<boost::csbl::tuple<boost::future<int> > > all = boost::when_any(boost::move(f1));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    // has exception
    //BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
  { // is_ready future copy-constructible
    boost::future<int> f1 = boost::make_ready_future(123);
    BOOST_TEST(f1.valid());
    BOOST_TEST(f1.is_ready());
    boost::future<boost::csbl::tuple<boost::future<int> > > all = boost::when_any(boost::move(f1));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(all.valid());
    if (0) // todo FAILS not yet implemented
    BOOST_TEST(all.is_ready());
    boost::csbl::tuple<boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
  { // is_ready shared_future copy-constructible
    boost::shared_future<int> f1 = boost::make_ready_future(123).share();
    BOOST_TEST(f1.valid());
    BOOST_TEST(f1.is_ready());
    boost::future<boost::csbl::tuple<boost::shared_future<int> > > all = boost::when_any(f1);
    BOOST_TEST(f1.valid());
    BOOST_TEST(all.valid());
    if (0) // todo FAILS not yet implemented
    BOOST_TEST(all.is_ready());
    boost::csbl::tuple<boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
  { // packaged_task future copy-constructible
    boost::packaged_task<int()> pt1(&p1);
    boost::future<int> f1 = pt1.get_future();
    BOOST_TEST(f1.valid());
    boost::future<boost::csbl::tuple<boost::future<int> > > all = boost::when_any(boost::move(f1));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(all.valid());
    pt1();
    boost::csbl::tuple<boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
  { // packaged_task shared_future copy-constructible
    boost::packaged_task<int()> pt1(&p1);
    boost::shared_future<int> f1 = pt1.get_future().share();
    BOOST_TEST(f1.valid());
    boost::future<boost::csbl::tuple<boost::shared_future<int> > > all = boost::when_any(f1);
    BOOST_TEST(f1.valid());
    BOOST_TEST(all.valid());
    pt1();
    boost::csbl::tuple<boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
  { // async future copy-constructible
    boost::future<int> f1 = boost::async(boost::launch::async, &p1);
    BOOST_TEST(f1.valid());
    boost::future<boost::csbl::tuple<boost::future<int> > > all = boost::when_any(boost::move(f1));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
  { // async shared_future copy-constructible
    boost::shared_future<int> f1 = boost::async(boost::launch::async, &p1).share();
    BOOST_TEST(f1.valid());
    boost::future<boost::csbl::tuple<boost::shared_future<int> > > all = boost::when_any(f1);
    BOOST_TEST(f1.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
#if defined BOOST_THREAD_PROVIDES_VARIADIC_THREAD
  // fixme darwin-4.8.0_11 terminate called without an active exception
  { // deferred future copy-constructible
    boost::future<int> f1 = boost::async(boost::launch::deferred, &p1);
    boost::future<boost::csbl::tuple<boost::future<int> > > all = boost::when_any(boost::move(f1));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
  // fixme darwin-4.8.0_11 terminate called without an active exception
  { // deferred shared_future copy-constructible
    boost::shared_future<int> f1 = boost::async(boost::launch::deferred, &p1).share();
    boost::future<boost::csbl::tuple<boost::shared_future<int> > > all = boost::when_any(f1);
    BOOST_TEST(f1.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
  }
#endif
#endif
  return boost::report_errors();
}

