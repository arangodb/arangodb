//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_TIMEOUT_SERVICE_HPP
#define BOOST_BEAST_CORE_IMPL_TIMEOUT_SERVICE_HPP

#include <boost/beast/experimental/core/detail/timeout_service.hpp>

namespace boost {
namespace beast {

inline
void
set_timeout_service_options(
    boost::asio::io_context& ioc,
    std::chrono::seconds interval)
{
    boost::asio::use_service<
        detail::timeout_service>(ioc).set_option(interval);
}

} // beast
} // boost

#endif
