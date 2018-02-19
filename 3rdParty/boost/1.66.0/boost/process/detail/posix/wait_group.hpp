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
#include <system_error>
#include <sys/types.h>
#include <sys/wait.h>

namespace boost { namespace process { namespace detail { namespace posix {

inline void wait(const group_handle &p)
{
    pid_t ret;
    int status;
    do
    {
        ret = ::waitpid(-p.grp, &status, 0);
    } while (((ret == -1) && (errno == EINTR)) || (ret != -1 && !WIFEXITED(status) && !WIFSIGNALED(status)));
    if (ret == -1)
        boost::process::detail::throw_last_error("waitpid(2) failed");
    if (WIFSIGNALED(status))
        throw process_error(std::error_code(), "process group terminated due to receipt of a signal");
}

inline void wait(const group_handle &p, std::error_code &ec) noexcept
{
    pid_t ret;
    int status;

    do
    {
        ret = ::waitpid(-p.grp, &status, 0);
    } 
    while (((ret == -1) && (errno == EINTR)) || (ret != -1 && !WIFEXITED(status) && !WIFSIGNALED(status)));
    
    if (ret == -1)
        ec = boost::process::detail::get_last_error();
    else if (WIFSIGNALED(status))
        ec = std::make_error_code(std::errc::no_such_process);
    else
        ec.clear();

}

template< class Rep, class Period >
inline bool wait_for(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& rel_time)
{

    pid_t ret;
    int status;

    auto start = std::chrono::system_clock::now();
    auto time_out = start + rel_time;

    bool time_out_occured = false;
    do
    {
        ret = ::waitpid(-p.grp, &status, WUNTRACED | WNOHANG);
        if (std::chrono::system_clock::now() >= time_out)
        {
            time_out_occured = true;
            break;
        }
    } 
    while (((ret == -1) && errno == EINTR)                               ||
           ((ret != -1) && !WIFEXITED(status) && !WIFSIGNALED(status)));


    if (ret == -1)
        boost::process::detail::throw_last_error("waitpid(2) failed");
    if (WIFSIGNALED(status))
        throw process_error(std::error_code(), "process group terminated due to receipt of a signal");

    return !time_out_occured;
}


template< class Rep, class Period >
inline bool wait_for(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& rel_time,
        std::error_code & ec) noexcept
{

    pid_t ret;
    int status;

    auto start = std::chrono::system_clock::now();
    auto time_out = start + rel_time;

    bool time_out_occured = false;
    do
    {
        ret = ::waitpid(-p.grp, &status, WUNTRACED | WNOHANG);
        if (std::chrono::system_clock::now() >= time_out)
        {
            time_out_occured = true;
            break;
        }
    } 
    while (((ret == -1) && errno == EINTR)                       ||
           ((ret != -1) && !WIFEXITED(status) && !WIFSIGNALED(status)));


    if (ret == -1)
        ec = boost::process::detail::get_last_error();
    else if (WIFSIGNALED(status))
        ec = std::make_error_code(std::errc::no_such_process);
    else
        ec.clear();

    return !time_out_occured;
}



template< class Rep, class Period >
inline bool wait_until(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& time_out)
{

    pid_t ret;
    int status;

    bool time_out_occured = false;
    do
    {
        ret = ::waitpid(-p.grp, &status, WUNTRACED | WNOHANG);
        if (std::chrono::system_clock::now() >= time_out)
        {
            time_out_occured = true;
            break;
        }
    } 
    while (((ret == -1) && errno == EINTR)                               ||
           ((ret != -1) && !WIFEXITED(status) && !WIFSIGNALED(status)));


    if (ret == -1)
        boost::process::detail::throw_last_error("waitpid(2) failed");
    if (WIFSIGNALED(status))
        throw process_error(std::error_code(), "process group terminated due to receipt of a signal");


    return !time_out_occured;
}


template< class Rep, class Period >
inline bool wait_until(
        const group_handle &p,
        const std::chrono::duration<Rep, Period>& time_out,
        std::error_code & ec) noexcept
{

    pid_t ret;
    int status;

    bool time_out_occured = false;
    do
    {
        ret = ::waitpid(-p.grp, &status, WUNTRACED | WNOHANG);
        if (std::chrono::system_clock::now() >= time_out)
        {
            time_out_occured = true;
            break;
        }
    } 
    while (((ret == -1) && errno == EINTR)                               ||
           ((ret != -1) && !WIFEXITED(status) && !WIFSIGNALED(status)));


    if (ret == -1)
        ec = boost::process::detail::get_last_error();
    else if (WIFSIGNALED(status))
        ec = std::make_error_code(std::errc::no_such_process);
    else
        ec.clear();

    return !time_out_occured;
}

}}}}

#endif
