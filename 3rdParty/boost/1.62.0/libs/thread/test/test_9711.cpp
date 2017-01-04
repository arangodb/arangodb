// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#include <boost/thread/future.hpp>


int main()
{
#if ! defined BOOST_NO_CXX11_LAMBDAS && ! (defined BOOST_MSVC && _MSC_VER < 1700)
    boost::promise<int> prom;
    boost::future<int> futr = prom.get_future();

    int callCount = 0;

    boost::future<void> futr2 = futr.then(boost::launch::deferred,
        [&] (boost::future<int> f) {
            callCount++;
            assert(f.valid());
            assert(f.is_ready());
            assert(17 == f.get());
        });

    assert(futr2.valid());
    assert(!futr2.is_ready());
    assert(0 == callCount);

    prom.set_value(17);
    assert(0 == callCount);

    futr2.get();
    assert(1 == callCount);

#endif
    return 0;
}
