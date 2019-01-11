//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffers_adapter.hpp>

#include "buffer_test.hpp"
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>
#include <iterator>

namespace boost {
namespace beast {

class buffers_adapter_test : public unit_test::suite
{
public:
    void testBuffersAdapter()
    {
        using boost::asio::buffer;
        using boost::asio::buffer_size;
        using boost::asio::const_buffer;
        using boost::asio::mutable_buffer;
        char buf[12];
        std::string const s = "Hello, world";
        BEAST_EXPECT(s.size() == sizeof(buf));
        for(std::size_t i = 1; i < 4; ++i) {
        for(std::size_t j = 1; j < 4; ++j) {
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        for(std::size_t t = 1; t < 4; ++ t) {
        for(std::size_t u = 1; u < 4; ++ u) {
        std::size_t k = sizeof(buf) - (i + j);
        std::size_t z = sizeof(buf) - (x + y);
        std::size_t v = sizeof(buf) - (t + u);
        {
            std::memset(buf, 0, sizeof(buf));
            std::array<mutable_buffer, 3> bs{{
                mutable_buffer{&buf[0], i},
                mutable_buffer{&buf[i], j},
                mutable_buffer{&buf[i+j], k}}};
            buffers_adapter<decltype(bs)> ba(std::move(bs));
            BEAST_EXPECT(ba.max_size() == sizeof(buf));
            {
                auto d = ba.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = ba.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = ba.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
                ba.commit(buffer_copy(d, buffer(s.data(), x)));
            }
            BEAST_EXPECT(ba.size() == x);
            BEAST_EXPECT(ba.max_size() == sizeof(buf));
            BEAST_EXPECT(buffer_size(ba.data()) == ba.size());
            {
                auto d = ba.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = ba.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = ba.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
                ba.commit(buffer_copy(d, buffer(s.data()+x, y)));
            }
            ba.commit(1);
            BEAST_EXPECT(ba.size() == x + y);
            BEAST_EXPECT(ba.max_size() == sizeof(buf));
            BEAST_EXPECT(buffer_size(ba.data()) == ba.size());
            {
                auto d = ba.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = ba.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = ba.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
                ba.commit(buffer_copy(d, buffer(s.data()+x+y, z)));
            }
            ba.commit(2);
            BEAST_EXPECT(ba.size() == x + y + z);
            BEAST_EXPECT(buffer_size(ba.data()) == ba.size());
            BEAST_EXPECT(buffers_to_string(ba.data()) == s);
            ba.consume(t);
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            BEAST_EXPECT(buffers_to_string(ba.data()) == s.substr(t, std::string::npos));
            ba.consume(u);
            BEAST_EXPECT(buffers_to_string(ba.data()) == s.substr(t + u, std::string::npos));
            ba.consume(v);
            BEAST_EXPECT(buffers_to_string(ba.data()) == "");
            ba.consume(1);
            {
                auto d = ba.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            try
            {
                ba.prepare(1);
                fail();
            }
            catch(...)
            {
                pass();
            }
        }
        }}}}}}
    }
    void testCommit()
    {
        using boost::asio::buffer_size;
        {
            using sb_type = boost::asio::streambuf;
            sb_type b;
            buffers_adapter<
                sb_type::mutable_buffers_type> ba(b.prepare(3));
            BEAST_EXPECT(buffer_size(ba.prepare(3)) == 3);
            ba.commit(2);
            BEAST_EXPECT(buffer_size(ba.data()) == 2);
        }
        {
            using sb_type = beast::multi_buffer;
            sb_type b;
            b.prepare(3);
            buffers_adapter<
                sb_type::mutable_buffers_type> ba(b.prepare(8));
            BEAST_EXPECT(buffer_size(ba.prepare(8)) == 8);
            ba.commit(2);
            BEAST_EXPECT(buffer_size(ba.data()) == 2);
            ba.consume(1);
            ba.commit(6);
            ba.consume(2);
            BEAST_EXPECT(buffer_size(ba.data()) == 5);
            ba.consume(5);
        }
    }

    void
    testIssue386()
    {
        using type = boost::asio::streambuf;
        type buffer;
        buffers_adapter<
            type::mutable_buffers_type> ba{buffer.prepare(512)};
        read_size(ba, 1024);
    }

    void run() override
    {
        testBuffersAdapter();
        testCommit();
        testIssue386();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_adapter);

} // beast
} // boost
