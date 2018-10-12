// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PROCESS_DETAIL_POSIX_WAIT_FOR_EXIT_HPP
#define BOOST_PROCESS_DETAIL_POSIX_WAIT_FOR_EXIT_HPP

#include <boost/process/detail/config.hpp>
#include <boost/process/detail/posix/child_handle.hpp>
#include <system_error>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace boost { namespace process { namespace detail { namespace posix {

inline void wait(const child_handle &p, int & exit_code, std::error_code &ec) noexcept
{
    pid_t ret;
    int status;

    do
    {
        ret = ::waitpid(p.pid, &status, 0);
    } 
    while (((ret == -1) && (errno == EINTR)) || (ret != -1 && !WIFEXITED(status) && !WIFSIGNALED(status)));

    if (ret == -1)
        ec = boost::process::detail::get_last_error();
    else
    {
        ec.clear();
        exit_code = status;
    }
}

inline void wait(const child_handle &p, int & exit_code) noexcept
{
    std::error_code ec;
    wait(p, exit_code, ec);
    boost::process::detail::throw_error(ec, "waitpid(2) failed in wait");
}

template< class Clock, class Duration >
inline bool wait_until(
        const child_handle &p,
        int & exit_code,
        const std::chrono::time_point<Clock, Duration>& time_out,
        std::error_code & ec) noexcept
{
    pid_t ret;
    int status;

    bool timed_out;

    do
    {
        ret = ::waitpid(p.pid, &status, WNOHANG);
        if (ret == 0)
        {
            timed_out = Clock::now() >= time_out;
            if (timed_out)
                return false;
        }
    }
    while ((ret == 0) ||
          (((ret == -1) && errno == EINTR) ||
           ((ret != -1) && !WIFEXITED(status) && !WIFSIGNALED(status))));

    if (ret == -1)
        ec = boost::process::detail::get_last_error();
    else
    {
        ec.clear();
        exit_code = status;
    }

    return true;
}

template< class Clock, class Duration >
inline bool wait_until(
        const child_handle &p,
        int & exit_code,
        const std::chrono::time_point<Clock, Duration>& time_out)
{
    std::error_code ec;
    bool b = wait_until(p, exit_code, time_out, ec);
    boost::process::detail::throw_error(ec, "waitpid(2) failed in wait_until");
    return b;
}

template< class Rep, class Period >
inline bool wait_for(
        const child_handle &p,
        int & exit_code,
        const std::chrono::duration<Rep, Period>& rel_time,
        std::error_code & ec) noexcept
{
    return wait_until(p, exit_code, std::chrono::steady_clock::now() + rel_time, ec);
}

template< class Rep, class Period >
inline bool wait_for(
        const child_handle &p,
        int & exit_code,
        const std::chrono::duration<Rep, Period>& rel_time)
{
    std::error_code ec;
    bool b = wait_for(p, exit_code, rel_time, ec);
    boost::process::detail::throw_error(ec, "waitpid(2) failed in wait_for");
    return b;
}

}}}}

#endif
