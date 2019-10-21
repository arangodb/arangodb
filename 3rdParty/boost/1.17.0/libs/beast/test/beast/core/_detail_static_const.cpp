//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/static_const.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

#include <boost/beast/core/close_socket.hpp>

namespace boost {
namespace beast {

struct foo_impl
{
    void operator()() const noexcept
    {
        BEAST_PASS();
    }
};

constexpr auto& bar =
    boost::beast::detail::static_const<detail::close_socket_impl>::value;

class static_const_test : public beast::unit_test::suite
{
public:
    void
    run() override
    {
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,static_const);

} // beast
} // boost
