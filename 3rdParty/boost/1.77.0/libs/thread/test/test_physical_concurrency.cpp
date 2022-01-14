// Copyright (C) 2007 Anthony Williams
// Copyright (C) 2013 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE Boost.Threads: physical_concurrency test suite

#include <boost/thread/thread_only.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/mutex.hpp>

BOOST_AUTO_TEST_CASE(test_physical_concurrency_is_non_zero)
{
#if defined(__MINGW32__) && !defined(__MINGW64__)
// This matches the condition in win32/thread.cpp, even though
// that's probably wrong on MinGW-w64 in 32 bit mode
#else
    BOOST_CHECK(boost::thread::physical_concurrency()!=0);
#endif
}




