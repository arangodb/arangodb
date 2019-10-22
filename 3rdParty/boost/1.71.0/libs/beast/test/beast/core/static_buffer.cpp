//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/static_buffer.hpp>

#include "test_buffer.hpp"

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <algorithm>
#include <cctype>
#include <string>

namespace boost {
namespace beast {

class static_buffer_test : public beast::unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        is_mutable_dynamic_buffer<
            static_buffer<13>>::value);

    BOOST_STATIC_ASSERT(
        is_mutable_dynamic_buffer<
            static_buffer_base>::value);

    void
    testDynamicBuffer()
    {
        test_dynamic_buffer(static_buffer<13>{});
    }

    void
    testMembers()
    {
        string_view const s = "Hello, world!";
        
        // static_buffer_base
        {
            char buf[64];
            static_buffer_base b{buf, sizeof(buf)};
            ostream(b) << s;
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
            b.clear();
            BEAST_EXPECT(b.size() == 0);
            BEAST_EXPECT(buffer_bytes(b.data()) == 0);
        }

        // static_buffer
        {
            static_buffer<64> b1;
            BEAST_EXPECT(b1.size() == 0);
            BEAST_EXPECT(b1.max_size() == 64);
            BEAST_EXPECT(b1.capacity() == 64);
            ostream(b1) << s;
            BEAST_EXPECT(buffers_to_string(b1.data()) == s);
            {
                static_buffer<64> b2{b1};
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
            {
                static_buffer<64> b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
        }

        // cause memmove
        {
            static_buffer<10> b;
            ostream(b) << "12345";
            b.consume(3);
            ostream(b) << "67890123";
            BEAST_EXPECT(buffers_to_string(b.data()) == "4567890123");
            try
            {
                b.prepare(1);
                fail("", __FILE__, __LINE__);
            }
            catch(std::length_error const&)
            {
                pass();
            }
        }

        // read_size
        {
            static_buffer<10> b;
            BEAST_EXPECT(read_size(b, 512) == 10);
            b.prepare(4);
            b.commit(4);
            BEAST_EXPECT(read_size(b, 512) == 6);
            b.consume(2);
            BEAST_EXPECT(read_size(b, 512) == 8);
            b.prepare(8);
            b.commit(8);
            BEAST_EXPECT(read_size(b, 512) == 0);
        }

        // base
        {
            static_buffer<10> b;
            [&](static_buffer_base& base)
            {
                BEAST_EXPECT(base.max_size() == b.capacity());
            }
            (b.base());

            [&](static_buffer_base const& base)
            {
                BEAST_EXPECT(base.max_size() == b.capacity());
            }
            (b.base());
        }

        // This exercises the wrap-around cases
        // for the circular buffer representation
        {
            static_buffer<5> b;
            {
                auto const mb = b.prepare(5);
                BEAST_EXPECT(buffers_length(mb) == 1);
            }
            b.commit(4);
            BEAST_EXPECT(buffers_length(b.data()) == 1);
            BEAST_EXPECT(buffers_length(b.cdata()) == 1);
            b.consume(3);
            {
                auto const mb = b.prepare(3);
                BEAST_EXPECT(buffers_length(mb) == 2);
                auto it1 = mb.begin();
                auto it2 = std::next(it1);
                BEAST_EXPECT(
                    net::const_buffer(*it1).data() >
                    net::const_buffer(*it2).data());
            }
            b.commit(2);
            {
                auto const mb = b.data();
                auto it1 = mb.begin();
                auto it2 = std::next(it1);
                BEAST_EXPECT(
                    net::const_buffer(*it1).data() >
                    net::const_buffer(*it2).data());
            }
            {
                auto const cb = b.cdata();
                auto it1 = cb.begin();
                auto it2 = std::next(it1);
                BEAST_EXPECT(
                    net::const_buffer(*it1).data() >
                    net::const_buffer(*it2).data());
            }
        }
    }

    void
    run() override
    {
        testDynamicBuffer();
        testMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,static_buffer);

} // beast
} // boost
