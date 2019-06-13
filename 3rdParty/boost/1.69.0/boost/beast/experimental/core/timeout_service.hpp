//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_TIMEOUT_SERVICE_HPP
#define BOOST_BEAST_CORE_TIMEOUT_SERVICE_HPP

//#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>

namespace boost {
namespace beast {

/** Set timeout service options in an execution context.

    This changes the time interval for all timeouts associated
    with the execution context. The option must be set before any
    timeout objects are constructed.

    @param ctx The execution context.

    @param interval The approximate amount of time until a timeout occurs.
*/
void
set_timeout_service_options(
    boost::asio::io_context& ctx, // VFALCO should be execution_context
    std::chrono::seconds interval);

} // beast
} // boost

#include <boost/beast/experimental/core/impl/timeout_service.ipp>

#endif
