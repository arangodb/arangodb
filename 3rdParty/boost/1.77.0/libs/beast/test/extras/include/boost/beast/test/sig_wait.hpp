//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_SIG_WAIT_HPP
#define BOOST_BEAST_TEST_SIG_WAIT_HPP

#include <boost/asio.hpp>

namespace boost {
namespace beast {
namespace test {

/// Block until SIGINT or SIGTERM is received.
inline
void
sig_wait()
{
    net::io_context ioc;
    net::signal_set signals(
        ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](boost::system::error_code const&, int)
        {
        });
    ioc.run();
}

} // test
} // beast
} // boost

#endif
