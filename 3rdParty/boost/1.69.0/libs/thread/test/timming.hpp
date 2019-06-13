// Copyright (C) 2018 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_THREAD_TEST_TIMMING_HPP
#define BOOST_THREAD_TEST_TIMMING_HPP

#include <boost/thread/detail/config.hpp>
#include <boost/detail/lightweight_test.hpp>

#if ! defined BOOST_THREAD_TEST_TIME_MS
#ifdef BOOST_THREAD_PLATFORM_WIN32
#define BOOST_THREAD_TEST_TIME_MS 250
#else
#define BOOST_THREAD_TEST_TIME_MS 75
#endif
#endif

#if ! defined BOOST_THREAD_TEST_TIME_WARNING
#define BOOST_THREAD_TEST_IT(A, B) BOOST_TEST_LT((A).count(), (B).count())
#else
#define BOOST_THREAD_TEST_IT(A, B) BOOST_TEST_LT((A).count(), (B).count())
#endif

#endif
