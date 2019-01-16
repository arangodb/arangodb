// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PROCESS_DETAIL_POSIX_WAIT_GROUP_HPP
#define BOOST_PROCESS_DETAIL_POSIX_WAIT_GROUP_HPP

#include <boost/process/detail/config.hpp>
#include <boost/process/detail/posix/group_handle.hpp>
#include <chrono>
#include <system_error>
#include <sys/types.h>
#include <sys/wait.h>

namespace boost { namespace process { namespace detail { namespace posix {

inline void wait(const group_handle &p, std::error_code &ec) noexcept
{
    pid_t ret;
    siginfo_t  status;

    do
    {
        ret = ::waitpid(-p.grp, &status.si_status, 0);
        if (ret == -1)
        {
            ec = get_last_error();
            return; 
        }

        //ECHILD --> no child processes left.
        ret = ::waitid(P_PGID, p.grp, &status, WEXITED | WNOHANG);
    } 
    while ((ret != -1) || (errno != ECHILD));
   
    if (errno != ECHILD)
        ec = boost::process::detail::get_last_error();
    else
        ec.clear();
}

inline void wait(const group_handle &p) noexcept
{
    std::error_code ec;
    wait(p, ec);
    boost::process::detail::throw_error(ec, "waitpid(2) failed in wait");
}

template< class Clock, class Duration >
inline bool wait_until(
        const group_handle &p,
        const std::chrono::time_point<Clock, Duration>& time_out,
        std::error_code & ec) noexcept
{

    ::sigset_t  sigset;
    ::siginfo_t siginfo;

    ::sigemptyset(&sigset);
    ::sigaddset(&sigset, SIGCHLD);

    auto get_timespec = 
            [](const Duration & dur)
            {
                ::timespec ts;
                ts.tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
                ts.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count() % 1000000000;
                return ts;
            };


    bool timed_out = false;
    int ret;

    struct ::sigaction old_sig;
    if (-1 == ::sigaction(SIGCHLD, nullptr, &old_sig))
    {
        ec = get_last_error();
        return false;
    }

    do
    {
        auto ts = get_timespec(time_out - Clock::now());
        ret = ::sigtimedwait(&sigset, nullptr, &ts);
        errno = 0;
        if ((ret == SIGCHLD) && (old_sig.sa_handler != SIG_DFL) && (old_sig.sa_handler != SIG_IGN))
            old_sig.sa_handler(ret);

        ret = ::waitpid(-p.grp, &siginfo.si_status, 0); //so in case it exited, we wanna reap it first
        if (ret == -1)
        {
            ec = get_last_error();
            return false; 
        }


        //check if we're done
        ret = ::waitid(P_PGID, p.grp, &siginfo, WEXITED | WNOHANG);

    } 
    while (((ret != -1) || (errno != ECHILD)) && !(timed_out = (Clock::now() > time_out)))  ;
   
    if (errno != ECHILD)
    {
        ec = boost::process::detail::get_last_error();
        return !timed_out;
    }
    else
    {
        ec.clear();
        return true; //even if timed out, there are no child proccessess left
    }

}

template< class Clock, class Duration >
inline bool wait_until(
        const group_handle &p,
        const std::chrono::time_point<Clock, Duration>& time_out) noexcept
{
    std::error_code ec;
    bool b = wait_until(p, time_out, ec);
    boost::process::detail::throw_error(ec, "waitpid(2) failed in wait_until");
    return b;
}

template< class Rep, class Period >
inline bool wait_for(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& rel_time,
        std::error_code & ec) noexcept
{
    return wait_until(p, std::chrono::steady_clock::now() + rel_time, ec);
}

template< class Rep, class Period >
inline bool wait_for(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& rel_time) noexcept
{
    std::error_code ec;
    bool b = wait_for(p, rel_time, ec);
    boost::process::detail::throw_error(ec, "waitpid(2) failed in wait_for");
    return b;
}

}}}}

#endif
