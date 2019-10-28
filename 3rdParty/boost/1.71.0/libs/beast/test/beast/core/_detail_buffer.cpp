//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
//#include <boost/beast/core/buffer.hpp>

#include <boost/beast/core/detail/buffer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <string>

#include <boost/asio/error.hpp>

namespace boost {
namespace beast {
namespace detail {

// VFALCO No idea why boost::system::errc::message_size fails
//        to compile, so we use net::error::eof instead.
//
class buffer_test : public beast::unit_test::suite
{
public:
    template<class DynamicBuffer>
    void
    testPrepare()
    {
    #ifndef BOOST_NO_EXCEPTIONS
        error_code ec;
        DynamicBuffer b(32);
        dynamic_buffer_prepare(b, 20, ec,
            net::error::eof);
        BEAST_EXPECTS(! ec, ec.message());
        b.commit(20);
        auto const result =
            dynamic_buffer_prepare(b, 20, ec,
            net::error::eof);
        BEAST_EXPECT(result == boost::none);
        BEAST_EXPECTS(
            ec == net::error::eof, ec.message());
    #else
        fail("exceptions disabled", __FILE__, __LINE__);
    #endif
    }

    template<class DynamicBuffer>
    void
    testPrepareNoexcept()
    {
        error_code ec;
        DynamicBuffer b(32);
        dynamic_buffer_prepare_noexcept(b, 20, ec,
            net::error::eof);
        BEAST_EXPECTS(! ec, ec.message());
        b.commit(20);
        auto const result =
            detail::dynamic_buffer_prepare_noexcept(b, 20, ec,
            net::error::eof);
        BEAST_EXPECT(result == boost::none);
        BEAST_EXPECTS(
            ec == net::error::eof, ec.message());
    }

    void
    run() override
    {
        testPrepare<flat_buffer>();
        testPrepareNoexcept<flat_buffer>();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffer);

} // detail
} // beast
} // boost
