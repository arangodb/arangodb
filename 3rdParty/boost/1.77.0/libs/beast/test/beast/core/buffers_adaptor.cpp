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

struct buffers_adaptor_test_hook
{
    template<class MutableBufferSequence>
    static
    auto
    make_subrange(
        buffers_adaptor <MutableBufferSequence> &adaptor,
        std::size_t pos = 0,
        std::size_t n = (std::numeric_limits<std::size_t>::max)())
    -> typename buffers_adaptor<MutableBufferSequence>::mutable_buffers_type
    {
        return adaptor.make_subrange(pos, n);
    }

    template<class MutableBufferSequence>
    static
    auto
    make_subrange(
        buffers_adaptor<MutableBufferSequence> const& adaptor,
        std::size_t pos = 0,
        std::size_t n = (std::numeric_limits<std::size_t>::max)())
    -> typename buffers_adaptor<MutableBufferSequence>::const_buffers_type
    {
        return adaptor.make_subrange(pos, n);
    }
};

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

    template<bool isMutable>
    void
    testSubrange()
    {
        std::string s =
            "the quick brown fox jumps over the lazy dog";

        auto iterate_test = [&](
            std::size_t a,
            std::size_t b,
            std::size_t c)
        {
            auto buffers = std::vector<net::mutable_buffer>();
            if (a)
                buffers.push_back(net::buffer(&s[0], a));
            if (b - a)
                buffers.push_back(net::buffer(&s[a], (b - a)));
            if (c - b)
                buffers.push_back(net::buffer(&s[b], (c - b)));
            auto adapter = buffers_adaptor<std::vector<net::mutable_buffer>>(buffers);

            using maybe_mutable =
            typename std::conditional<
                isMutable,
                buffers_adaptor<std::vector<net::mutable_buffer>>&,
                buffers_adaptor<std::vector<net::mutable_buffer>> const&>::type;

            auto sub = buffers_adaptor_test_hook::make_subrange(static_cast<maybe_mutable>(adapter));
            /*
            using value_type = typename std::conditional<
                isMutable, net::mutable_buffer, net::const_buffer>::type;
            BEAST_EXPECTS(typeid(typename decltype(sub)::value_type) == typeid(value_type), "iterate_test");
            */
            BEAST_EXPECT(buffers_to_string(sub) == s.substr(0, c));
        };

        iterate_test(0, 0, 1);

        for (std::size_t a = 0; a <= s.size(); ++a)
            for (std::size_t b = a; b <= s.size(); ++b)
                for (std::size_t c = b; c <= s.size(); ++c)
                    iterate_test(a, b, c);
    }


    void
    run() override
    {
        testDynamicBuffer();
        testSpecial();
        testIssue386();
        testSubrange<true>();
        testSubrange<false>();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_adaptor);

} // beast
} // boost
