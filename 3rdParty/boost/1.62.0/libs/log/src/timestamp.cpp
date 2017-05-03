/*
 *          Copyright Andrey Semashev 2007 - 2015.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   timestamp.cpp
 * \author Andrey Semashev
 * \date   31.07.2011
 *
 * \brief  This header is the Boost.Log library implementation, see the library documentation
 *         at http://www.boost.org/doc/libs/release/libs/log/doc/html/index.html.
 */

#include <boost/log/detail/config.hpp>
#include <boost/log/detail/timestamp.hpp>

#if defined(BOOST_WINDOWS) && !defined(__CYGWIN__)
#if !defined(BOOST_LOG_NO_THREADS)
#include <boost/memory_order.hpp>
#include <boost/atomic/atomic.hpp>
#endif
#include <boost/detail/winapi/dll.hpp>
#include <boost/detail/winapi/time.hpp>
#else
#include <unistd.h> // for config macros
#if defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
#include <mach/mach_time.h>
#include <mach/kern_return.h>
#include <boost/log/utility/once_block.hpp>
#include <boost/system/error_code.hpp>
#endif
#include <time.h>
#include <errno.h>
#include <boost/throw_exception.hpp>
#include <boost/log/exceptions.hpp>
#endif
#include <boost/log/detail/header.hpp>

namespace boost {

BOOST_LOG_OPEN_NAMESPACE

namespace aux {

#if defined(BOOST_WINDOWS) && !defined(__CYGWIN__)

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

// Directly use API from Vista and later
BOOST_LOG_API get_tick_count_t get_tick_count = &boost::detail::winapi::GetTickCount64;

#else // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

BOOST_LOG_ANONYMOUS_NAMESPACE {

// Zero-initialized initially
#if !defined(BOOST_LOG_NO_THREADS)
static BOOST_ALIGNMENT(BOOST_LOG_CPU_CACHE_LINE_SIZE) boost::atomic< uint64_t > g_ticks;
#else
static BOOST_ALIGNMENT(BOOST_LOG_CPU_CACHE_LINE_SIZE) uint64_t g_ticks;
#endif

//! Artifical implementation of GetTickCount64
uint64_t WINAPI get_tick_count64()
{
#if !defined(BOOST_LOG_NO_THREADS)
    uint64_t old_state = g_ticks.load(boost::memory_order_acquire);
#else
    uint64_t old_state = g_ticks;
#endif

    uint32_t new_ticks = boost::detail::winapi::GetTickCount();

    uint32_t old_ticks = static_cast< uint32_t >(old_state & UINT64_C(0x00000000ffffffff));
    uint64_t new_state = ((old_state & UINT64_C(0xffffffff00000000)) + (static_cast< uint64_t >(new_ticks < old_ticks) << 32)) | static_cast< uint64_t >(new_ticks);

#if !defined(BOOST_LOG_NO_THREADS)
    g_ticks.store(new_state, boost::memory_order_release);
#else
    g_ticks = new_state;
#endif

    return new_state;
}

uint64_t WINAPI get_tick_count_init()
{
    boost::detail::winapi::HMODULE_ hKernel32 = boost::detail::winapi::GetModuleHandleW(L"kernel32.dll");
    if (hKernel32)
    {
        get_tick_count_t p = (get_tick_count_t)boost::detail::winapi::get_proc_address(hKernel32, "GetTickCount64");
        if (p)
        {
            // Use native API
            get_tick_count = p;
            return p();
        }
    }

    // No native API available
    get_tick_count = &get_tick_count64;
    return get_tick_count64();
}

} // namespace

BOOST_LOG_API get_tick_count_t get_tick_count = &get_tick_count_init;

#endif // BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6

#elif (defined(_POSIX_TIMERS) && (_POSIX_TIMERS+0) > 0)  /* POSIX timers supported */ \
      || defined(__GNU__) || defined(__OpenBSD__) || defined(__CloudABI__)  /* GNU Hurd, OpenBSD and Nuxi CloudABI don't support POSIX timers fully but do provide clock_gettime() */

BOOST_LOG_API int64_t duration::milliseconds() const
{
    // Timestamps are always in nanoseconds
    return m_ticks / INT64_C(1000000);
}

BOOST_LOG_ANONYMOUS_NAMESPACE {

/*!
 * \c get_timestamp implementation based on POSIX realtime clock.
 * Note that this implementation is only used as a last resort since
 * this timer can be manually set and may jump due to DST change.
 */
timestamp get_timestamp_realtime_clock()
{
    timespec ts;
    if (BOOST_UNLIKELY(clock_gettime(CLOCK_REALTIME, &ts) != 0))
    {
        const int err = errno;
        BOOST_LOG_THROW_DESCR_PARAMS(system_error, "Failed to acquire current time", (err));
    }

    return timestamp(static_cast< uint64_t >(ts.tv_sec) * UINT64_C(1000000000) + ts.tv_nsec);
}

#   if defined(_POSIX_MONOTONIC_CLOCK)

//! \c get_timestamp implementation based on POSIX monotonic clock
timestamp get_timestamp_monotonic_clock()
{
    timespec ts;
    if (BOOST_UNLIKELY(clock_gettime(CLOCK_MONOTONIC, &ts) != 0))
    {
        const int err = errno;
        if (err == EINVAL)
        {
            // The current platform does not support monotonic timer.
            // Fall back to realtime clock, which is not exactly what we need
            // but is better than nothing.
            get_timestamp = &get_timestamp_realtime_clock;
            return get_timestamp_realtime_clock();
        }
        BOOST_LOG_THROW_DESCR_PARAMS(system_error, "Failed to acquire current time", (err));
    }

    return timestamp(static_cast< uint64_t >(ts.tv_sec) * UINT64_C(1000000000) + ts.tv_nsec);
}

#       define BOOST_LOG_DEFAULT_GET_TIMESTAMP get_timestamp_monotonic_clock

#   else // if defined(_POSIX_MONOTONIC_CLOCK)
#       define BOOST_LOG_DEFAULT_GET_TIMESTAMP get_timestamp_realtime_clock
#   endif // if defined(_POSIX_MONOTONIC_CLOCK)

} // namespace

// Use POSIX API
BOOST_LOG_API get_timestamp_t get_timestamp = &BOOST_LOG_DEFAULT_GET_TIMESTAMP;

#   undef BOOST_LOG_DEFAULT_GET_TIMESTAMP

#elif defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)

BOOST_LOG_API int64_t duration::milliseconds() const
{
    static mach_timebase_info_data_t timebase_info = {};
    BOOST_LOG_ONCE_BLOCK()
    {
        kern_return_t err = mach_timebase_info(&timebase_info);
        if (err != KERN_SUCCESS)
        {
            BOOST_LOG_THROW_DESCR_PARAMS(system_error, "Failed to initialize timebase info", (boost::system::errc::not_supported));
        }
    }

    // Often the timebase rational equals 1, we can optimize for this case
    if (timebase_info.numer == timebase_info.denom)
    {
        // Timestamps are in nanoseconds
        return m_ticks / INT64_C(1000000);
    }
    else
    {
        return (m_ticks * timebase_info.numer) / (INT64_C(1000000) * timebase_info.denom);
    }
}

BOOST_LOG_ANONYMOUS_NAMESPACE {

//! \c get_timestamp implementation based on MacOS X absolute time
timestamp get_timestamp_mach()
{
    return timestamp(mach_absolute_time());
}

} // namespace

// Use MacOS X API
BOOST_LOG_API get_timestamp_t get_timestamp = &get_timestamp_mach;

#else

#   error Boost.Log: Timestamp generation is not supported for your platform

#endif

} // namespace aux

BOOST_LOG_CLOSE_NAMESPACE // namespace log

} // namespace boost

#include <boost/log/detail/footer.hpp>
