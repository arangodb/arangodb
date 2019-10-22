//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_prefix.hpp>

#include "test_buffer.hpp"

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <string>

namespace boost {
namespace beast {

class buffers_prefix_test : public beast::unit_test::suite
{
public:
    void
    testBufferSequence()
    {
        char buf[13];
        auto const b =
            buffers_triple(buf, sizeof(buf));
        for(std::size_t i = 1; i <= sizeof(buf); ++i)
            test_buffer_sequence(
                buffers_prefix(i, b));
    }

    void
    testInPlaceInit()
    {
        {
            class test_buffers
            {
                net::const_buffer cb_;

            public:
                using const_iterator =
                    net::const_buffer const*;

                explicit
                test_buffers(std::true_type)
                {
                }

                const_iterator
                begin() const
                {
                    return &cb_;
                }

                const_iterator
                end() const
                {
                    return begin() + 1;
                }
            };
            buffers_prefix_view<test_buffers> v(
                2, boost::in_place_init, std::true_type{});
            BEAST_EXPECT(buffer_bytes(v) == 0);
        }

        {
            char c[2];
            c[0] = 0;
            c[1] = 0;
            buffers_prefix_view<net::const_buffer> v(
                2, boost::in_place_init, c, sizeof(c));
            BEAST_EXPECT(buffer_bytes(v) == 2);
        }

        {
            char c[2];
            buffers_prefix_view<net::mutable_buffer> v(
                2, boost::in_place_init, c, sizeof(c));
            BEAST_EXPECT(buffer_bytes(v) == 2);
        }
    }

    template<class BufferType>
    void
    testPrefixes()
    {
        std::string s = "Hello, world";
        BEAST_EXPECT(s.size() == 12);
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        {
            std::size_t z = s.size() - (x + y);
            std::array<BufferType, 3> bs{{
                BufferType{&s[0], x},
                BufferType{&s[x], y},
                BufferType{&s[x+y], z}}};
            for(std::size_t i = 0; i <= s.size() + 1; ++i)
            {
                auto pb = buffers_prefix(i, bs);
                BEAST_EXPECT(buffers_to_string(pb) == s.substr(0, i));
                auto pb2 = pb;
                BEAST_EXPECT(buffers_to_string(pb2) == buffers_to_string(pb));
                pb = buffers_prefix(0, bs);
                pb2 = pb;
                BEAST_EXPECT(buffer_bytes(pb2) == 0);
                pb2 = buffers_prefix(i, bs);
                BEAST_EXPECT(buffers_to_string(pb2) == s.substr(0, i));
            }
        }
        } }
    }

    void testEmpty()
    {
        auto pb0 = buffers_prefix(0, net::mutable_buffer{});
        BEAST_EXPECT(buffer_bytes(pb0) == 0);
        auto pb1 = buffers_prefix(1, net::mutable_buffer{});
        BEAST_EXPECT(buffer_bytes(pb1) == 0);
        BEAST_EXPECT(net::buffer_copy(pb0, pb1) == 0);
    }

    void
    testBuffersFront()
    {
        {
            std::array<net::const_buffer, 2> v;
            v[0] = {"", 0};
            v[1] = net::const_buffer("Hello, world!", 13);
            BEAST_EXPECT(buffers_front(v).size() == 0);
            std::swap(v[0], v[1]);
            BEAST_EXPECT(buffers_front(v).size() == 13);
        }
        {
            struct null_sequence
            {
                net::const_buffer b;
                using iterator = net::const_buffer const*;
                iterator begin() const noexcept
                {
                    return &b;
                }
                iterator end() const noexcept
                {
                    return begin();
                }
            };
            null_sequence z;
            BEAST_EXPECT(buffers_front(z).size() == 0);
        }
    }

    void
    run() override
    {
        testBufferSequence();
        testInPlaceInit();
        testPrefixes<net::const_buffer>();
        testPrefixes<net::mutable_buffer>();
        testEmpty();
        testBuffersFront();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_prefix);

} // beast
} // boost
