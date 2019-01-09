//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/multi_buffer.hpp>

#include "buffer_test.hpp"

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/test/test_allocator.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

namespace boost {
namespace beast {

BOOST_STATIC_ASSERT(
    boost::asio::is_dynamic_buffer<multi_buffer>::value);

class multi_buffer_test : public beast::unit_test::suite
{
public:
    template<class Alloc1, class Alloc2>
    static
    bool
    eq(basic_multi_buffer<Alloc1> const& mb1,
        basic_multi_buffer<Alloc2> const& mb2)
    {
        return buffers_to_string(mb1.data()) ==
            buffers_to_string(mb2.data());
    }

    template<class ConstBufferSequence>
    void
    expect_size(std::size_t n, ConstBufferSequence const& buffers)
    {
        BEAST_EXPECT(test::size_pre(buffers) == n);
        BEAST_EXPECT(test::size_post(buffers) == n);
        BEAST_EXPECT(test::size_rev_pre(buffers) == n);
        BEAST_EXPECT(test::size_rev_post(buffers) == n);
    }

    template<class U, class V>
    static
    void
    self_assign(U& u, V&& v)
    {
        u = std::forward<V>(v);
    }

    void
    testMatrix1()
    {
        using namespace test;
        using boost::asio::buffer;
        std::string const s = "Hello, world";
        BEAST_EXPECT(s.size() == 12);
        for(std::size_t i = 1; i < 12; ++i) {
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        std::size_t z = s.size() - (x + y);
        {
            multi_buffer b;
            b.commit(buffer_copy(b.prepare(x), buffer(s.data(), x)));
            b.commit(buffer_copy(b.prepare(y), buffer(s.data()+x, y)));
            b.commit(buffer_copy(b.prepare(z), buffer(s.data()+x+y, z)));
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
            {
                multi_buffer mb2{b};
                BEAST_EXPECT(eq(b, mb2));
            }
            {
                multi_buffer mb2;
                mb2 = b;
                BEAST_EXPECT(eq(b, mb2));
            }
            {
                multi_buffer mb2{std::move(b)};
                BEAST_EXPECT(buffers_to_string(mb2.data()) == s);
                expect_size(0, b.data());
                b = std::move(mb2);
                BEAST_EXPECT(buffers_to_string(b.data()) == s);
                expect_size(0, mb2.data());
            }
            self_assign(b, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
            self_assign(b, std::move(b));
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        }
        }}}
    }

    void
    testMatrix2()
    {
        using namespace test;
        using boost::asio::buffer;
        using boost::asio::buffer_size;
        std::string const s = "Hello, world";
        BEAST_EXPECT(s.size() == 12);
        for(std::size_t i = 1; i < 12; ++i) {
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        for(std::size_t t = 1; t < 4; ++ t) {
        for(std::size_t u = 1; u < 4; ++ u) {
        std::size_t z = s.size() - (x + y);
        std::size_t v = s.size() - (t + u);
        {
            multi_buffer b;
            {
                auto d = b.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = b.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = b.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
                b.commit(buffer_copy(d, buffer(s.data(), x)));
            }
            BEAST_EXPECT(b.size() == x);
            BEAST_EXPECT(buffer_size(b.data()) == b.size());
            {
                auto d = b.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = b.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
            }
            {
                auto d = b.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
                b.commit(buffer_copy(d, buffer(s.data()+x, y)));
            }
            b.commit(1);
            BEAST_EXPECT(b.size() == x + y);
            BEAST_EXPECT(buffer_size(b.data()) == b.size());
            {
                auto d = b.prepare(x);
                BEAST_EXPECT(buffer_size(d) == x);
            }
            {
                auto d = b.prepare(y);
                BEAST_EXPECT(buffer_size(d) == y);
            }
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            {
                auto d = b.prepare(z);
                BEAST_EXPECT(buffer_size(d) == z);
                b.commit(buffer_copy(d, buffer(s.data()+x+y, z)));
            }
            b.commit(2);
            BEAST_EXPECT(b.size() == x + y + z);
            BEAST_EXPECT(buffer_size(b.data()) == b.size());
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
            b.consume(t);
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
            BEAST_EXPECT(buffers_to_string(b.data()) == s.substr(t, std::string::npos));
            b.consume(u);
            BEAST_EXPECT(buffers_to_string(b.data()) == s.substr(t + u, std::string::npos));
            b.consume(v);
            BEAST_EXPECT(buffers_to_string(b.data()) == "");
            b.consume(1);
            {
                auto d = b.prepare(0);
                BEAST_EXPECT(buffer_size(d) == 0);
            }
        }
        }}}}}
    }

    void
    testIterators()
    {
        using boost::asio::buffer_size;
        multi_buffer b;
        b.prepare(1);
        b.commit(1);
        b.prepare(2);
        b.commit(2);
        expect_size(3, b.data());
        b.prepare(1);
        expect_size(3, b.prepare(3));
        b.commit(2);
    }

    void
    testMembers()
    {
        using namespace test;

        // compare equal
        using equal_t = test::test_allocator<char,
            true, true, true, true, true>;

        // compare not equal
        using unequal_t = test::test_allocator<char,
            false, true, true, true, true>;

        // construction
        {
            {
                multi_buffer b;
                BEAST_EXPECT(b.capacity() == 0);
            }
            {
                multi_buffer b{500};
                BEAST_EXPECT(b.capacity() == 0);
                BEAST_EXPECT(b.max_size() == 500);
            }
            {
                unequal_t a1;
                basic_multi_buffer<unequal_t> b{a1};
                BEAST_EXPECT(b.get_allocator() == a1);
                BEAST_EXPECT(b.get_allocator() != unequal_t{});
            }
        }

        // move construction
        {
            {
                basic_multi_buffer<equal_t> b1{30};
                BEAST_EXPECT(b1.get_allocator()->nmove == 0);
                ostream(b1) << "Hello";
                basic_multi_buffer<equal_t> b2{std::move(b1)};
                BEAST_EXPECT(b2.get_allocator()->nmove == 1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
            // allocators equal
            {
                basic_multi_buffer<equal_t> b1{30};
                ostream(b1) << "Hello";
                equal_t a;
                basic_multi_buffer<equal_t> b2{std::move(b1), a};
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
            {
                // allocators unequal
                basic_multi_buffer<unequal_t> b1{30};
                ostream(b1) << "Hello";
                unequal_t a;
                basic_multi_buffer<unequal_t> b2{std::move(b1), a};
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                BEAST_EXPECT(b1.max_size() == b2.max_size());
            }
        }

        // copy construction
        {
            {
                basic_multi_buffer<equal_t> b1;
                ostream(b1) << "Hello";
                basic_multi_buffer<equal_t> b2{b1};
                BEAST_EXPECT(b1.get_allocator() == b2.get_allocator());
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                basic_multi_buffer<unequal_t> b1;
                ostream(b1) << "Hello";
                unequal_t a;
                basic_multi_buffer<unequal_t> b2(b1, a);
                BEAST_EXPECT(b1.get_allocator() != b2.get_allocator());
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                basic_multi_buffer<equal_t> b1;
                ostream(b1) << "Hello";
                basic_multi_buffer<unequal_t> b2(b1);
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                basic_multi_buffer<unequal_t> b1;
                ostream(b1) << "Hello";
                equal_t a;
                basic_multi_buffer<equal_t> b2(b1, a);
                BEAST_EXPECT(b2.get_allocator() == a);
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
        }

        // move assignment
        {
            {
                multi_buffer b1;
                ostream(b1) << "Hello";
                multi_buffer b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.capacity() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                // propagate_on_container_move_assignment : true
                using pocma_t = test::test_allocator<char,
                    true, true, true, true, true>;
                basic_multi_buffer<pocma_t> b1;
                ostream(b1) << "Hello";
                basic_multi_buffer<pocma_t> b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                // propagate_on_container_move_assignment : false
                using pocma_t = test::test_allocator<char,
                    true, true, false, true, true>;
                basic_multi_buffer<pocma_t> b1;
                ostream(b1) << "Hello";
                basic_multi_buffer<pocma_t> b2;
                b2 = std::move(b1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
        }

        // copy assignment
        {
            {
                multi_buffer b1;
                ostream(b1) << "Hello";
                multi_buffer b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                basic_multi_buffer<equal_t> b3;
                b3 = b2;
                BEAST_EXPECT(buffers_to_string(b3.data()) == "Hello");
            }
            {
                // propagate_on_container_copy_assignment : true
                using pocca_t = test::test_allocator<char,
                    true, true, true, true, true>;
                basic_multi_buffer<pocca_t> b1;
                ostream(b1) << "Hello";
                basic_multi_buffer<pocca_t> b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
            {
                // propagate_on_container_copy_assignment : false
                using pocca_t = test::test_allocator<char,
                    true, false, true, true, true>;
                basic_multi_buffer<pocca_t> b1;
                ostream(b1) << "Hello";
                basic_multi_buffer<pocca_t> b2;
                b2 = b1;
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
            }
        }

        // prepare
        {
            {
                multi_buffer b{100};
                try
                {
                    b.prepare(b.max_size() + 1);
                    fail("", __FILE__, __LINE__);
                }
                catch(std::length_error const&)
                {
                    pass();
                }
            }
            {
                string_view const s = "Hello, world!";
                multi_buffer b1{64};
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(b1.max_size() == 64);
                BEAST_EXPECT(b1.capacity() == 0);
                ostream(b1) << s;
                BEAST_EXPECT(buffers_to_string(b1.data()) == s);
                {
                    multi_buffer b2{b1};
                    BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                    b2.consume(7);
                    BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
                }
                {
                    multi_buffer b2{64};
                    b2 = b1;
                    BEAST_EXPECT(buffers_to_string(b2.data()) == s);
                    b2.consume(7);
                    BEAST_EXPECT(buffers_to_string(b2.data()) == s.substr(7));
                }
            }
            {
                multi_buffer b;
                b.prepare(1000);
                BEAST_EXPECT(b.capacity() >= 1000);
                b.commit(1);
                BEAST_EXPECT(b.size() == 1);
                BEAST_EXPECT(b.capacity() >= 1000);
                b.prepare(1000);
                BEAST_EXPECT(b.size() == 1);
                BEAST_EXPECT(b.capacity() >= 1000);
                b.prepare(1500);
                BEAST_EXPECT(b.capacity() >= 1000);
            }
            {
                multi_buffer b;
                b.prepare(1000);
                BEAST_EXPECT(b.capacity() >= 1000);
                b.commit(1);
                BEAST_EXPECT(b.capacity() >= 1000);
                b.prepare(1000);
                BEAST_EXPECT(b.capacity() >= 1000);
                b.prepare(2000);
                BEAST_EXPECT(b.capacity() >= 2000);
                b.commit(2);
            }
            {
                multi_buffer b;
                b.prepare(1000);
                BEAST_EXPECT(b.capacity() >= 1000);
                b.prepare(2000);
                BEAST_EXPECT(b.capacity() >= 2000);
                b.prepare(4000);
                BEAST_EXPECT(b.capacity() >= 4000);
                b.prepare(50);
                BEAST_EXPECT(b.capacity() >= 50);
            }
        }

        // commit
        {
            multi_buffer b;
            b.prepare(1000);
            BEAST_EXPECT(b.capacity() >= 1000);
            b.commit(1000);
            BEAST_EXPECT(b.size() == 1000);
            BEAST_EXPECT(b.capacity() >= 1000);
            b.consume(1000);
            BEAST_EXPECT(b.size() == 0);
            BEAST_EXPECT(b.capacity() == 0);
            b.prepare(1000);
            b.commit(650);
            BEAST_EXPECT(b.size() == 650);
            BEAST_EXPECT(b.capacity() >= 1000);
            b.prepare(1000);
            BEAST_EXPECT(b.capacity() >= 1650);
            b.commit(100);
            BEAST_EXPECT(b.size() == 750);
            BEAST_EXPECT(b.capacity() >= 1000);
            b.prepare(1000);
            BEAST_EXPECT(b.capacity() >= 2000);
            b.commit(500);
        }

        // consume
        {
            multi_buffer b;
            b.prepare(1000);
            BEAST_EXPECT(b.capacity() >= 1000);
            b.commit(1000);
            BEAST_EXPECT(b.size() == 1000);
            BEAST_EXPECT(b.capacity() >= 1000);
            b.prepare(1000);
            BEAST_EXPECT(b.capacity() >= 2000);
            b.commit(750);
            BEAST_EXPECT(b.size() == 1750);
            b.consume(500);
            BEAST_EXPECT(b.size() == 1250);
            b.consume(500);
            BEAST_EXPECT(b.size() == 750);
            b.prepare(250);
            b.consume(750);
            BEAST_EXPECT(b.size() == 0);
            b.prepare(1000);
            b.commit(800);
            BEAST_EXPECT(b.size() == 800);
            b.prepare(1000);
            b.commit(600);
            BEAST_EXPECT(b.size() == 1400);
            b.consume(1400);
            BEAST_EXPECT(b.size() == 0);
        }

        // swap
        {
            {
                // propagate_on_container_swap : true
                using pocs_t = test::test_allocator<char,
                    false, true, true, true, true>;
                pocs_t a1, a2;
                BEAST_EXPECT(a1 != a2);
                basic_multi_buffer<pocs_t> b1{a1};
                ostream(b1) << "Hello";
                basic_multi_buffer<pocs_t> b2{a2};
                BEAST_EXPECT(b1.get_allocator() == a1);
                BEAST_EXPECT(b2.get_allocator() == a2);
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator() == a2);
                BEAST_EXPECT(b2.get_allocator() == a1);
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator() == a1);
                BEAST_EXPECT(b2.get_allocator() == a2);
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(b2.size() == 0);
            }
            {
                // propagate_on_container_swap : false
                using pocs_t = test::test_allocator<char,
                    true, true, true, false, true>;
                pocs_t a1, a2;
                BEAST_EXPECT(a1 == a2);
                BEAST_EXPECT(a1.id() != a2.id());
                basic_multi_buffer<pocs_t> b1{a1};
                ostream(b1) << "Hello";
                basic_multi_buffer<pocs_t> b2{a2};
                BEAST_EXPECT(b1.get_allocator() == a1);
                BEAST_EXPECT(b2.get_allocator() == a2);
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator().id() == a1.id());
                BEAST_EXPECT(b2.get_allocator().id() == a2.id());
                BEAST_EXPECT(b1.size() == 0);
                BEAST_EXPECT(buffers_to_string(b2.data()) == "Hello");
                swap(b1, b2);
                BEAST_EXPECT(b1.get_allocator().id() == a1.id());
                BEAST_EXPECT(b2.get_allocator().id() == a2.id());
                BEAST_EXPECT(buffers_to_string(b1.data()) == "Hello");
                BEAST_EXPECT(b2.size() == 0);
            }
        }

        // read_size
        {
            multi_buffer b{10};
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
    }

    void
    run() override
    {
        testMatrix1();
        testMatrix2();
        testIterators();
        testMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,multi_buffer);

} // beast
} // boost
