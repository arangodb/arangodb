//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_adaptor.hpp>

#include "test_buffer.hpp"

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>
#include <iterator>

namespace boost {
namespace beast {

class buffers_adaptor_test : public unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        is_mutable_dynamic_buffer<
            buffers_adaptor<buffers_triple>>::value);

    void
    testDynamicBuffer()
    {
        char s[13];
        buffers_triple tb(s, sizeof(s));
        buffers_adaptor<buffers_triple> b(tb);
        test_dynamic_buffer(b);
    }

    void
    testSpecial()
    {
        char s1[13];
        buffers_triple tb1(s1, sizeof(s1));
        BEAST_EXPECT(buffer_bytes(tb1) == sizeof(s1));

        char s2[15];
        buffers_triple tb2(s2, sizeof(s2));
        BEAST_EXPECT(buffer_bytes(tb2) == sizeof(s2));

        {
            // construction

            buffers_adaptor<buffers_triple> b1(tb1);
            BEAST_EXPECT(b1.value() == tb1);

            buffers_adaptor<buffers_triple> b2(tb2);
            BEAST_EXPECT(b2.value() == tb2);

            buffers_adaptor<buffers_triple> b3(b2);
            BEAST_EXPECT(b3.value() == tb2);

            char s3[15];
            buffers_adaptor<buffers_triple> b4(
                boost::in_place_init, s3, sizeof(s3));
            BEAST_EXPECT(b4.value() == buffers_triple(s3, sizeof(s3)));

            // assignment

            b3 = b1;
            BEAST_EXPECT(b3.value() == tb1);
        }
    }

    void
    testIssue386()
    {
        using type = net::streambuf;
        type buffer;
        buffers_adaptor<
            type::mutable_buffers_type> ba{buffer.prepare(512)};
        read_size(ba, 1024);
    }

    void
    run() override
    {
        testDynamicBuffer();
        testSpecial();
        testIssue386();
#if 0
        testBuffersAdapter();
        testCommit();
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_adaptor);

} // beast
} // boost
