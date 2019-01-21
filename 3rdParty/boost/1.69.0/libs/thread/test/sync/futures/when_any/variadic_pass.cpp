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

// template <class T, class Ts>
// future<tuple<T, Ts...>> when_any(T&&, Ts&& ...);

#include <boost/config.hpp>

#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4

#include <boost/thread/future.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <stdexcept>

#ifdef BOOST_MSVC
#pragma warning(disable: 4127) // conditional expression is constant
#endif

int p1()
{
  return 123;
}

int thr()
{
  throw std::logic_error("123");
}
int p2()
{
  boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
  return 321;
}

int main()
{
#if defined BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
  if (0) // todo not yet implemented
  { // invalid future copy-constructible
    boost::future<int> f1;
    boost::future<int> f2 = boost::make_ready_future(321);
    BOOST_TEST(! f1.valid());
    BOOST_TEST(f2.valid());
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    // has exception
    //BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // is_ready future copy-constructible
    boost::future<int> f1 = boost::make_ready_future(123);
    boost::future<int> f2 = boost::make_ready_future(321);
    BOOST_TEST(f1.valid());
    BOOST_TEST(f1.is_ready());
    BOOST_TEST(f2.valid());
    BOOST_TEST(f2.is_ready());
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    if (0) // todo FAILS not yet implemented
    BOOST_TEST(all.is_ready());
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // is_ready shared_future copy-constructible
    boost::shared_future<int> f1 = boost::make_ready_future(123).share();
    boost::shared_future<int> f2 = boost::make_ready_future(321).share();
    BOOST_TEST(f1.valid());
    BOOST_TEST(f1.is_ready());
    BOOST_TEST(f2.valid());
    BOOST_TEST(f2.is_ready());
    boost::future<boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > > all = boost::when_any(f1, f2);
    BOOST_TEST(f1.valid());
    BOOST_TEST(f2.valid());
    BOOST_TEST(all.valid());
    if (0) // todo FAILS not yet implemented
    BOOST_TEST(all.is_ready());
    boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // packaged_task future copy-constructible
    boost::packaged_task<int()> pt1(&p1);
    boost::future<int> f1 = pt1.get_future();
    BOOST_TEST(f1.valid());
    boost::packaged_task<int()> pt2(&p2);
    boost::future<int> f2 = pt2.get_future();
    BOOST_TEST(f2.valid());
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    pt1();
    pt2();
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // packaged_task future copy-constructible
    boost::packaged_task<int()> pt1(&thr);
    boost::future<int> f1 = pt1.get_future();
    BOOST_TEST(f1.valid());
    boost::packaged_task<int()> pt2(&p2);
    boost::future<int> f2 = pt2.get_future();
    BOOST_TEST(f2.valid());
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    pt1();
    pt2();
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    try {
      boost::csbl::get<0>(res).get();
      BOOST_TEST(false);
    } catch (std::logic_error& ex) {
      BOOST_TEST(ex.what() == std::string("123"));
    } catch (...) {
      BOOST_TEST(false);
    }
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // packaged_task shared_future copy-constructible
    boost::packaged_task<int()> pt1(&p1);
    boost::shared_future<int> f1 = pt1.get_future().share();
    BOOST_TEST(f1.valid());
    boost::packaged_task<int()> pt2(&p2);
    boost::shared_future<int> f2 = pt2.get_future().share();
    BOOST_TEST(f2.valid());
    boost::future<boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > > all = boost::when_any(f1, f2);
    BOOST_TEST(f1.valid());
    BOOST_TEST(f2.valid());
    BOOST_TEST(all.valid());
    BOOST_TEST(! all.is_ready());
    pt1();
    pt2();
    boost::this_thread::sleep_for(boost::chrono::milliseconds(300));
    BOOST_TEST(all.is_ready());
    boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // async future copy-constructible
    boost::future<int> f1 = boost::async(boost::launch::async, &p1);
    BOOST_TEST(f1.valid());
    boost::future<int> f2 = boost::async(boost::launch::async, &p2);
    BOOST_TEST(f2.valid());
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // async shared_future copy-constructible
    boost::shared_future<int> f1 = boost::async(boost::launch::async, &p1).share();
    BOOST_TEST(f1.valid());
    boost::shared_future<int> f2 = boost::async(boost::launch::async, &p2).share();
    BOOST_TEST(f2.valid());
    boost::future<boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > > all = boost::when_any(f1, f2);
    BOOST_TEST(f1.valid());
    BOOST_TEST(f2.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  { // async future copy-constructible
    boost::future<int> f1 = boost::async(boost::launch::async, &p1);
    BOOST_TEST(f1.valid());
    boost::future<int> f2 = boost::make_ready_future(321);
    BOOST_TEST(f2.valid());
    BOOST_TEST(f2.is_ready());
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    //BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
#if defined BOOST_THREAD_PROVIDES_VARIADIC_THREAD
  // fixme darwin-4.8.0_11 terminate called without an active exception
  { // deferred future copy-constructible
    boost::future<int> f1 = boost::async(boost::launch::deferred, &p1);
    boost::future<int> f2 = boost::async(boost::launch::deferred, &p2);
    std::cout << __FILE__ << " " << __LINE__ << std::endl;

    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    std::cout << __FILE__ << " " << __LINE__ << std::endl;
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    std::cout << __FILE__ << " " << __LINE__ << std::endl;
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    std::cout << __FILE__ << " " << __LINE__ << std::endl;
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  // fixme darwin-4.8.0_11 terminate called without an active exception
  { // deferred future copy-constructible
    boost::future<int> f1 = boost::async(boost::launch::deferred, &p1);
    boost::future<int> f2 = boost::async(boost::launch::async, &p2);
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    //BOOST_TEST(! boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  // fixme darwin-4.8.0_11 terminate called without an active exception
  { // deferred future copy-constructible
    boost::future<int> f1 = boost::async(boost::launch::async, &p1);
    boost::future<int> f2 = boost::async(boost::launch::deferred, &p2);
    boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
    BOOST_TEST(! f1.valid());
    BOOST_TEST(! f2.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::future<int>,boost::future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    //BOOST_TEST(boost::csbl::get<0>(res).is_ready());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
  // fixme darwin-4.8.0_11 terminate called without an active exception
  { // deferred shared_future copy-constructible
    boost::shared_future<int> f1 = boost::async(boost::launch::deferred, &p1).share();
    boost::shared_future<int> f2 = boost::async(boost::launch::deferred, &p2).share();
    boost::future<boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > > all = boost::when_any(f1, f2);
    BOOST_TEST(f1.valid());
    BOOST_TEST(f2.valid());
    BOOST_TEST(all.valid());
    boost::csbl::tuple<boost::shared_future<int>,boost::shared_future<int> > res = all.get();
    BOOST_TEST(boost::csbl::get<0>(res).valid());
    BOOST_TEST(boost::csbl::get<1>(res).valid());
    BOOST_TEST(boost::csbl::get<0>(res).is_ready() || boost::csbl::get<1>(res).is_ready());
    BOOST_TEST(boost::csbl::get<0>(res).get() == 123);
    BOOST_TEST(boost::csbl::get<1>(res).get() == 321);
  }
#endif
#if ! defined BOOST_NO_CXX11_LAMBDAS
    { // async futures copy-constructible then()
      boost::future<int> f1 = boost::async(boost::launch::async, &p1);
      BOOST_TEST(f1.valid());
      boost::future<int> f2 = boost::async(boost::launch::async, &p2);
      BOOST_TEST(f2.valid());
      boost::future<boost::csbl::tuple<boost::future<int>,boost::future<int> > > all = boost::when_any(boost::move(f1), boost::move(f2));
      BOOST_TEST(! f1.valid());
      BOOST_TEST(! f2.valid());
      BOOST_TEST(all.valid());
      boost::future<int> sum = all.then([](boost::future<boost::csbl::tuple<boost::future<int>, boost::future<int> > > f)
      {
        boost::csbl::tuple<boost::future<int>,boost::future<int> > v = f.get();
        return boost::csbl::get<0>(v).get()+boost::csbl::get<1>(v).get();
      });
      BOOST_TEST(sum.valid());
      BOOST_TEST(sum.get() == 444);
    }
#endif
#endif

  return boost::report_errors();
}

