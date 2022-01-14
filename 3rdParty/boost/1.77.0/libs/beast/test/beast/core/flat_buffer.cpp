//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/flat_buffer.hpp>

#include "test_buffer.hpp"

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/test/test_allocator.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <algorithm>
#include <cctype>

namespace boost {
namespace beast {

class flat_buffer_test : public beast::unit_test::suite
{
public:
    BOOST_STATIC_ASSERT(
        is_mutable_dynamic_buffer<flat_buffer>::value);

    void
    testDynamicBuffer()
    {
        flat_buffer b(30);
        BEAST_EXPECT(b.max_size() == 30);
        test_dynamic_buffer(b);
    }

    void
    testSpecialMembers()
    {
        using namespace test;

        using a_t = test::test_allocator<char,
            true, true, true, true, true>;

        // Equal == false
        using a_neq_t = test::test_allocator<char,
            false, true, true, true, true>;

        // construction
        {
            {
                flat_buffer b;
                BEAST_EXPECT(b.capacity() == 0);
            }
            {
                flat_buffer b{500};
                BEAST_EXPECT(b.capacity() == 0);
                BEAST_EXPECT(b.max_size() == 500);
            }
            {
                a_neq_t a1;
                basic_flat_buffer<a_neq_t> b{a1};
                BEAST_EXPECT(b.get_allocator() == a1);
                a_neq_t a2;
                BEAST_EXPECT(b.get_allocator() != a2);
            }
            {
                a_neq_t a;
                basic_flat_buffer<a_neq_t> b{500, a};
                BEAST_EXPECT(b.capacity() == 0);
                BEAST_EXPECT(b.max_size() == 500);
            }
        }

        // move construction
        {
            {
                basic_flat_buffer<a_t> b1{30};
                BEAST_EXPECT(b1.get_allocator()->nmove == 0);
                ostream(b1) << "Hello";
                basic_flat_buffer<a_t> b2{std::move(b1)};
                BEAST_EXPECT(b2.get_allocator()->nmove == 1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
            {
                basic_flat_buffer<a_t> b1{30};
                ostream(b1) << "Hello";
                a_t a;
                basic_flat_buffer<a_t> b2{std::move(b1), a};
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
            {
                basic_flat_buffer<a_neq_t> b1{30};
                ostream(b1) << "Hello";
                a_neq_t a;
                basic_flat_buffer<a_neq_t> b2{std::move(b1), a};
                BEAST_EXPECT(b1.size() != 0);
                BEAST_EXPECT(b1.capacity() != 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
        }

        // copy construction
        {
            basic_flat_buffer<a_t> b1;
            ostream(b1) << "Hello";
            basic_flat_buffer<a_t> b2(b1);
            BEAST_EXPECT(b1.get_allocator() == b2.get_allocator());
            BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
            BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
        }
        {
            basic_flat_buffer<a_neq_t> b1;
            ostream(b1) << "Hello";
            a_neq_t a;
            basic_flat_buffer<a_neq_t> b2(b1, a);
            BEAST_EXPECT(b1.get_allocator() != b2.get_allocator());
            BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
            BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
        }
        {
            basic_flat_buffer<a_t> b1;
            ostream(b1) << "Hello";
            basic_flat_buffer<a_neq_t> b2(b1);
            BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
            BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
        }
        {
            basic_flat_buffer<a_neq_t> b1;
            ostream(b1) << "Hello";
            a_t a;
            basic_flat_buffer<a_t> b2(b1, a);
            BEAST_EXPECT(b2.get_allocator() == a);
            BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
            BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
        }
        {
            flat_buffer b1;
            ostream(b1) << "Hello";
            basic_flat_buffer<a_t> b2;
            b2.reserve(1);
            BEAST_EXPECT(b2.capacity() == 1);
            b2 = b1;
            BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            BEAST_EXPECT(b2.capacity() == b2.size());
        }

        // move assignment
        {
            {
                flat_buffer b1;
                ostream(b1) << "Hello";
                flat_buffer b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                using na_t = test::test_allocator<char,
                    true, true, false, true, true>;
                basic_flat_buffer<na_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<na_t> b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.get_allocator() == b2.get_allocator());
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                using na_t = test::test_allocator<char,
                    false, true, false, true, true>;
                basic_flat_buffer<na_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<na_t> b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.get_allocator() != b2.get_allocator());
                BEAST_EXPECT(b1.size() != 0);
                BEAST_EXPECT(b1.capacity() != 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                // propagate_on_container_move_assignment : true
                using pocma_t = test::test_allocator<char,
                    true, true, true, true, true>;
                basic_flat_buffer<pocma_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<pocma_t> b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                // propagate_on_container_move_assignment : false
                using pocma_t = test::test_allocator<char,
                    true, true, false, true, true>;
                basic_flat_buffer<pocma_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<pocma_t> b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
        }

        // copy assignment
        {
            {
                flat_buffer b1;
                ostream(b1) << "Hello";
                flat_buffer b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                basic_flat_buffer<a_t> b3;
                b3 = b2;
                BEAST_EXPECT(buffers_to_string(b3.data()) == "Hello");
            }
            {
                // propagate_on_container_copy_assignment : true
                using pocca_t = test::test_allocator<char,
                    true, true, true, true, true>;
                basic_flat_buffer<pocca_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<pocca_t> b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                // propagate_on_container_copy_assignment : false
                using pocca_t = test::test_allocator<char,
                    true, false, true, true, true>;
                basic_flat_buffer<pocca_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<pocca_t> b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
        }

        // operations
        {
            string_view const s = "Hello, world!";
            flat_buffer b1{64};
            BEAST_EXPECT(b1.size() == 0);
            BEAST_EXPECT(b1.max_size() == 64);
            BEAST_EXPECT(b1.capacity() == 0);
            ostream(b1) << s;
            BEAST_EXPECT(buffers_to_string(b1.data()) == s);
            {
                flat_buffer b2{b1};
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
            {
                flat_buffer b2{32};
                BEAST_EXPECT(b2.max_size() == 32);
                b2 = b1;
                BEAST_EXPECT(b2.max_size() == b1.max_size());
                BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                b2.consume(7);
                BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
            }
        }

        // cause memmove
        {
            flat_buffer b{20};
            ostream(b) << "12345";
            b.consume(3);
            ostream(b) << "67890123";
            BEAST_EXPECT(buffers_to_string(b.data()) == "4567890123");
        }

        // max_size
        {
            flat_buffer b{10};
            BEAST_EXPECT(b.max_size() == 10);
            b.max_size(32);
            BEAST_EXPECT(b.max_size() == 32);
        }

        // allocator max_size
        {
            basic_flat_buffer<a_t> b;
            auto a = b.get_allocator();
            BOOST_STATIC_ASSERT(
                ! std::is_const<decltype(a)>::value);
            a->max_size = 30;
            try
            {
                b.prepare(1000);
                fail("", __FILE__, __LINE__);
            }
            catch(std::length_error const&)
            {
                pass();
            }
        }

        // read_size
        {
            flat_buffer b{10};
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

        // swap
        {
            {
                basic_flat_buffer<a_neq_t> b1;
                ostream(b1) << "Hello";
                basic_flat_buffer<a_neq_t> b2;
                BEAST_EXPECT(b1.get_allocator() != b2.get_allocator());
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator() != b2.get_allocator());
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                using na_t = test::test_allocator<char,
                    true, true, true, false, true>;
                na_t a1;
                basic_flat_buffer<na_t> b1{a1};
                na_t a2;
                ostream(b1) << "Hello";
                basic_flat_buffer<na_t> b2{a2};
                BEAST_EXPECT(b1.get_allocator() == a1);
                BEAST_EXPECT(b2.get_allocator() == a2);
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator() == b2.get_allocator());
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
        }

        // prepare
        {
            flat_buffer b{100};
            b.prepare(10);
            b.commit(10);
            b.prepare(5);
            BEAST_EXPECT(b.capacity() >= 5);
            try
            {
                b.prepare(1000);
                fail("", __FILE__, __LINE__);
            }
            catch(std::length_error const&)
            {
                pass();
            }
        }

        // reserve
        {
            flat_buffer b;
            BEAST_EXPECT(b.capacity() == 0);
            b.reserve(50);
            BEAST_EXPECT(b.capacity() == 50);
            b.prepare(20);
            b.commit(20);
            b.reserve(50);
            BEAST_EXPECT(b.capacity() == 50);

            b.max_size(b.capacity());
            b.reserve(b.max_size() + 20);
            BEAST_EXPECT(b.capacity() == 70);
            BEAST_EXPECT(b.max_size() == 70);
        }

        // shrink to fit
        {
            flat_buffer b;
            BEAST_EXPECT(b.capacity() == 0);
            b.prepare(50);
            BEAST_EXPECT(b.capacity() == 50);
            b.commit(50);
            BEAST_EXPECT(b.capacity() == 50);
            b.prepare(75);
            BEAST_EXPECT(b.capacity() >= 125);
            b.shrink_to_fit();
            BEAST_EXPECT(b.capacity() == b.size());
            b.shrink_to_fit();
            BEAST_EXPECT(b.capacity() == b.size());
            b.consume(b.size());
            BEAST_EXPECT(b.size() == 0);
            b.shrink_to_fit();
            BEAST_EXPECT(b.capacity() == 0);
        }

        // clear
        {
            flat_buffer b;
            BEAST_EXPECT(b.capacity() == 0);
            b.prepare(50);
            b.commit(50);
            BEAST_EXPECT(b.size() == 50);
            BEAST_EXPECT(b.capacity() == 50);
            b.clear();
            BEAST_EXPECT(b.size() == 0);
            BEAST_EXPECT(b.capacity() == 50);
        }
    }

    void
    run() override
    {
        testDynamicBuffer();
        testSpecialMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,flat_buffer);

} // beast
} // boost
