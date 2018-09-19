// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PROCESS_DETAIL_WINDOWS_WAIT_GROUP_HPP_
#define BOOST_PROCESS_DETAIL_WINDOWS_WAIT_GROUP_HPP_

#include <boost/process/detail/config.hpp>
#include <boost/winapi/jobs.hpp>
#include <boost/winapi/wait.hpp>
#include <chrono>

namespace boost { namespace process { namespace detail { namespace windows {

struct group_handle;

inline void wait(const group_handle &p, std::error_code &ec)
{
    if (::boost::winapi::WaitForSingleObject(p.handle(),
        ::boost::winapi::infinite) == ::boost::winapi::wait_failed)
            ec = get_last_error();
}

inline void wait(const group_handle &p)
{
    std::error_code ec;
    wait(p, ec);
    boost::process::detail::throw_error(ec, "wait error");
}

template< class Clock, class Duration >
inline bool wait_until(
        const group_handle &p,
        const std::chrono::time_point<Clock, Duration>& timeout_time,
        std::error_code &ec)
{
    std::chrono::milliseconds ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                    timeout_time - Clock::now());

    ::boost::winapi::DWORD_ wait_code;
    wait_code = ::boost::winapi::WaitForSingleObject(p.handle(), ms.count());

    if (wait_code == ::boost::winapi::wait_failed)
        ec = get_last_error();

    else if (wait_code == ::boost::winapi::wait_timeout)
        return false; //

    return true;
}

template< class Clock, class Duration >
inline bool wait_until(
        const group_handle &p,
        const std::chrono::time_point<Clock, Duration>& timeout_time)
{
    std::error_code ec;
    bool b = wait_until(p, timeout_time, ec);
    boost::process::detail::throw_error(ec, "wait_until error");
    return b;
}

template< class Rep, class Period >
inline bool wait_for(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& rel_time,
        std::error_code &ec)
{
    return wait_until(p, std::chrono::steady_clock::now() + rel_time, ec);
}

template< class Rep, class Period >
inline bool wait_for(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& rel_time)
{
    std::error_code ec;
    bool b = wait_for(p, rel_time, ec);
    boost::process::detail::throw_error(ec, "wait_for error");
    return b;
}

}}}}

#endif /* BOOST_PROCESS_DETAIL_WINDOWS_WAIT_GROUP_HPP_ */
