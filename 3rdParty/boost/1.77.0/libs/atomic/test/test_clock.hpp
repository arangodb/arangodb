//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TEST_TEST_CLOCK_HPP_INCLUDED_
#define BOOST_ATOMIC_TEST_TEST_CLOCK_HPP_INCLUDED_

#include <boost/config.hpp>
#if defined(BOOST_WINDOWS)
#include <boost/winapi/config.hpp>
#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/time.hpp>
#include <boost/ratio/ratio.hpp>
#endif
#include <boost/chrono/chrono.hpp>

namespace chrono = boost::chrono;

#if defined(BOOST_WINDOWS)

// On Windows high precision clocks tend to cause spurious test failures because threads wake up earlier than expected.
// Use a lower precision steady clock for tests.
struct test_clock
{
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
    typedef boost::winapi::ULONGLONG_ rep;
#else
    typedef boost::winapi::DWORD_ rep;
#endif
    typedef boost::milli period;
    typedef chrono::duration< rep, period > duration;
    typedef chrono::time_point< test_clock, duration > time_point;

    static BOOST_CONSTEXPR_OR_CONST bool is_steady = true;

    static time_point now() BOOST_NOEXCEPT
    {
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
        rep ticks = boost::winapi::GetTickCount64();
#else
        rep ticks = boost::winapi::GetTickCount();
#endif
        return time_point(duration(ticks));
    }
};

#elif defined(BOOST_CHRONO_HAS_CLOCK_STEADY)
typedef chrono::steady_clock test_clock;
#else
typedef chrono::system_clock test_clock;
#endif

#endif // BOOST_ATOMIC_TEST_TEST_CLOCK_HPP_INCLUDED_
