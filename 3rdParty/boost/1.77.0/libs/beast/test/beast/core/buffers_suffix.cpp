//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_suffix.hpp>

#include "test_buffer.hpp"

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <string>

namespace boost {
namespace beast {

class buffers_suffix_test : public beast::unit_test::suite
{
public:
    void
    testBufferSequence()
    {
        // mutable
        {
            char buf[13];
            auto b = buffers_triple(buf, sizeof(buf));
            buffers_suffix<decltype(b)> bs(b);
            test_buffer_sequence(bs);
        }

        // const
        {
            string_view src = "Hello, world!";
            std::array<net::const_buffer, 3> b{{
                net::const_buffer(src.data(),     3),
                net::const_buffer(src.data() + 3, 4),
                net::const_buffer(src.data() + 7, 6) }};
            buffers_suffix<decltype(b)> bs(b);
            test_buffer_sequence(bs);
        }
    }

    void
    testSpecial()
    {
        // default construction
        {
            class test_buffer
                : public net::const_buffer
            {
            public:
                test_buffer()
                    : net::const_buffer("\r\n", 2)
                {
                }
            };

            buffers_suffix<test_buffer> cb;
            BEAST_EXPECT(buffers_to_string(cb) == "\r\n");
            cb.consume(1);
            BEAST_EXPECT(buffers_to_string(cb) == "\n");
        }

        // in-place init
        {
            buffers_suffix<buffers_cat_view<
                net::const_buffer,
                net::const_buffer>> cb(
                    boost::in_place_init,
                        net::const_buffer("\r", 1),
                        net::const_buffer("\n", 1));
            BEAST_EXPECT(buffers_to_string(cb) == "\r\n");
        }

        // empty sequence
        {
            buffers_suffix<net::mutable_buffer> cb(
                net::mutable_buffer{});
            BEAST_EXPECT(buffer_bytes(cb) == 0);
            buffers_suffix<net::mutable_buffer> cb2(
                net::mutable_buffer{});
            BEAST_EXPECT(net::buffer_copy(cb2, cb) == 0);
        }
    }

    template<class BufferSequence>
    static
    buffers_suffix<BufferSequence>
    consumed_buffers(BufferSequence const& bs, std::size_t n)
    {
        buffers_suffix<BufferSequence> cb(bs);
        cb.consume(n);
        return cb;
    }

    template<class Buffers1, class Buffers2>
    static
    bool
    eq(Buffers1 const& lhs, Buffers2 const& rhs)
    {
        return
            buffers_to_string(lhs) ==
            buffers_to_string(rhs);
    }

    void
    testMatrix()
    {
        char buf[12];
        std::string const s = "Hello, world";
        BEAST_EXPECT(s.size() == sizeof(buf));
        net::buffer_copy(net::buffer(buf), net::buffer(s));
        BEAST_EXPECT(buffers_to_string(net::buffer(buf)) == s);
        for(std::size_t i = 1; i < 4; ++i) {
        for(std::size_t j = 1; j < 4; ++j) {
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        std::size_t k = sizeof(buf) - (i + j);
        std::size_t z = sizeof(buf) - (x + y);
        {
            std::array<net::const_buffer, 3> bs{{
                net::const_buffer{&buf[0], i},
                net::const_buffer{&buf[i], j},
                net::const_buffer{&buf[i+j], k}}};
            buffers_suffix<decltype(bs)> cb(bs);
            BEAST_EXPECT(buffers_to_string(cb) == s);
            BEAST_EXPECT(buffer_bytes(cb) == s.size());
            cb.consume(0);
            BEAST_EXPECT(eq(cb, consumed_buffers(bs, 0)));
            BEAST_EXPECT(buffers_to_string(cb) == s);
            BEAST_EXPECT(buffer_bytes(cb) == s.size());
            cb.consume(x);
            BEAST_EXPECT(buffers_to_string(cb) == s.substr(x));
            BEAST_EXPECT(eq(cb, consumed_buffers(bs, x)));
            cb.consume(y);
            BEAST_EXPECT(buffers_to_string(cb) == s.substr(x+y));
            BEAST_EXPECT(eq(cb, consumed_buffers(bs, x+y)));
            cb.consume(z);
            BEAST_EXPECT(buffers_to_string(cb) == "");
            BEAST_EXPECT(eq(cb, consumed_buffers(bs, x+y+z)));
            cb.consume(1);
            BEAST_EXPECT(buffers_to_string(cb) == "");
            BEAST_EXPECT(eq(cb, consumed_buffers(bs, x+y+z)));
        }
        }}}}
    }

    void
    run() override
    {
        testBufferSequence();
        testSpecial();
        testMatrix();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_suffix);

} // beast
} // boost
