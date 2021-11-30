//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/flat_static_buffer.hpp>

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

class flat_static_buffer_test : public beast::unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        is_mutable_dynamic_buffer<
            flat_static_buffer<13>>::value);

    void
    testDynamicBuffer()
    {
        test_dynamic_buffer(flat_static_buffer<13>{});
    }

    void
    testMembers()
    {
        string_view const s = "Hello, world!";
        
        // flat_static_buffer_base
        {
            char buf[64];
            flat_static_buffer_base b{buf, sizeof(buf)};
            ostream(b) << s;
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
            b.clear();
            BEAST_EXPECT(b.size() == 0);
            BEAST_EXPECT(buffer_bytes(b.data()) == 0);
        }

        // flat_static_buffer
        {
            flat_static_buffer<64> b1;
            BEAST_EXPECT(b1.size() == 0);
            BEAST_EXPECT(b1.max_size() == 64);
            BEAST_EXPECT(b1.capacity() == 64);
            ostream(b1) << s;
            BEAST_EXPECT(buffers_to_string(b1.data()) == s);
            {
                flat_static_buffer<64> b2{b1};
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
            {
                flat_static_buffer<64> b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
        }

        // cause memmove
        {
            flat_static_buffer<10> b;
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
            flat_static_buffer<10> b;
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
            flat_static_buffer<10> b;
            [&](flat_static_buffer_base& base)
            {
                BEAST_EXPECT(base.max_size() == b.capacity());
            }
            (b.base());

            [&](flat_static_buffer_base const& base)
            {
                BEAST_EXPECT(base.max_size() == b.capacity());
            }
            (b.base());
        }
    }

    void run() override
    {
        testDynamicBuffer();
        testMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,flat_static_buffer);

} // beast
} // boost
