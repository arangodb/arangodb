//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/fields.hpp>

#include <boost/beast/core/static_string.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/type_traits.hpp>
#include <boost/beast/test/test_allocator.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <string>

namespace boost {
namespace beast {
namespace http {

class fields_test : public beast::unit_test::suite
{
public:
    static constexpr std::size_t max_static_buffer =
        sizeof(beast::detail::temporary_buffer);

    template<class T>
    class test_allocator
    {
    public:
        using value_type = T;

        test_allocator() noexcept(false) {}

        template<class U, class = typename
            std::enable_if<!std::is_same<test_allocator, U>::value>::type>
        test_allocator(test_allocator<U> const&) noexcept {}

        value_type*
        allocate(std::size_t n)
        {
            return static_cast<value_type*>(::operator new (n*sizeof(value_type)));
        }

        void
        deallocate(value_type* p, std::size_t) noexcept
        {
            ::operator delete(p);
        }

        template<class U>
        friend
        bool
        operator==(test_allocator<T> const&, test_allocator<U> const&) noexcept
        {
            return true;
        }

        template<class U>
        friend
        bool
        operator!=(test_allocator<T> const& x, test_allocator<U> const& y) noexcept
        {
            return !(x == y);
        }
    };

    using test_fields = basic_fields<test_allocator<char>>;

    BOOST_STATIC_ASSERT(is_fields<fields>::value);
    BOOST_STATIC_ASSERT(is_fields<test_fields>::value);

    // std::allocator is noexcept movable, fields should satisfy
    // these constraints as well.
    BOOST_STATIC_ASSERT(std::is_nothrow_move_constructible<fields>::value);
    BOOST_STATIC_ASSERT(std::is_nothrow_move_assignable<fields>::value);

    // Check if basic_fields respects throw-constructibility and
    // propagate_on_container_move_assignment of the allocator.
    BOOST_STATIC_ASSERT(std::is_nothrow_move_constructible<test_fields>::value);
    BOOST_STATIC_ASSERT(!std::is_nothrow_move_assignable<test_fields>::value);

    template<class Allocator>
    using fa_t = basic_fields<Allocator>;

    using f_t = fa_t<std::allocator<char>>;

    template<class Allocator>
    static
    void
    fill(std::size_t n, basic_fields<Allocator>& f)
    {
        for(std::size_t i = 1; i<= n; ++i)
            f.insert(std::to_string(i), to_static_string(i));
    }

    template<class U, class V>
    static
    void
    self_assign(U& u, V&& v)
    {
        u = std::forward<V>(v);
    }

    template<class Alloc>
    static
    bool
    empty(basic_fields<Alloc> const& f)
    {
        return f.begin() == f.end();
    }

    template<class Alloc>
    static
    std::size_t
    size(basic_fields<Alloc> const& f)
    {
        return std::distance(f.begin(), f.end());
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
                fields f;
                BEAST_EXPECT(f.begin() == f.end());
            }
            {
                unequal_t a1;
                basic_fields<unequal_t> f{a1};
                BEAST_EXPECT(f.get_allocator() == a1);
                BEAST_EXPECT(f.get_allocator() != unequal_t{});
            }
        }

        // move construction
        {
            {
                basic_fields<equal_t> f1;
                BEAST_EXPECT(f1.get_allocator()->nmove == 0);
                f1.insert("1", "1");
                BEAST_EXPECT(f1["1"] == "1");
                basic_fields<equal_t> f2{std::move(f1)};
                BEAST_EXPECT(f2.get_allocator()->nmove == 1);
                BEAST_EXPECT(f2["1"] == "1");
                BEAST_EXPECT(f1["1"] == "");
            }
            // allocators equal
            {
                basic_fields<equal_t> f1;
                f1.insert("1", "1");
                equal_t a;
                basic_fields<equal_t> f2{std::move(f1), a};
                BEAST_EXPECT(f2["1"] == "1");
                BEAST_EXPECT(f1["1"] == "");
            }
            {
                // allocators unequal
                basic_fields<unequal_t> f1;
                f1.insert("1", "1");
                unequal_t a;
                basic_fields<unequal_t> f2{std::move(f1), a};
                BEAST_EXPECT(f2["1"] == "1");
            }
        }

        // copy construction
        {
            {
                basic_fields<equal_t> f1;
                f1.insert("1", "1");
                basic_fields<equal_t> f2{f1};
                BEAST_EXPECT(f1.get_allocator() == f2.get_allocator());
                BEAST_EXPECT(f1["1"] == "1");
                BEAST_EXPECT(f2["1"] == "1");
            }
            {
                basic_fields<unequal_t> f1;
                f1.insert("1", "1");
                unequal_t a;
                basic_fields<unequal_t> f2(f1, a);
                BEAST_EXPECT(f1.get_allocator() != f2.get_allocator());
                BEAST_EXPECT(f1["1"] == "1");
                BEAST_EXPECT(f2["1"] == "1");
            }
            {
                basic_fields<equal_t> f1;
                f1.insert("1", "1");
                basic_fields<unequal_t> f2(f1);
                BEAST_EXPECT(f1["1"] == "1");
                BEAST_EXPECT(f2["1"] == "1");
            }
            {
                basic_fields<unequal_t> f1;
                f1.insert("1", "1");
                equal_t a;
                basic_fields<equal_t> f2(f1, a);
                BEAST_EXPECT(f2.get_allocator() == a);
                BEAST_EXPECT(f1["1"] == "1");
                BEAST_EXPECT(f2["1"] == "1");
            }
        }

        // move assignment
        {
            {
                fields f1;
                f1.insert("1", "1");
                fields f2;
                f2 = std::move(f1);
                BEAST_EXPECT(f1.begin() == f1.end());
                BEAST_EXPECT(f2["1"] == "1");
            }
            {
                // propagate_on_container_move_assignment : true
                using pocma_t = test::test_allocator<char,
                    true, true, true, true, true>;
                basic_fields<pocma_t> f1;
                f1.insert("1", "1");
                basic_fields<pocma_t> f2;
                f2 = std::move(f1);
                BEAST_EXPECT(f1.begin() == f1.end());
                BEAST_EXPECT(f2["1"] == "1");
            }
            {
                // propagate_on_container_move_assignment : false
                using pocma_t = test::test_allocator<char,
                    true, true, false, true, true>;
                basic_fields<pocma_t> f1;
                f1.insert("1", "1");
                basic_fields<pocma_t> f2;
                f2 = std::move(f1);
                BEAST_EXPECT(f1.begin() == f1.end());
                BEAST_EXPECT(f2["1"] == "1");
            }
        }

        // copy assignment
        {
            {
                fields f1;
                f1.insert("1", "1");
                fields f2;
                f2 = f1;
                BEAST_EXPECT(f1["1"] == "1");
                BEAST_EXPECT(f2["1"] == "1");
                basic_fields<equal_t> f3;
                f3 = f2;
                BEAST_EXPECT(f3["1"] == "1");
            }
            {
                // propagate_on_container_copy_assignment : true
                using pocca_t = test::test_allocator<char,
                    true, true, true, true, true>;
                basic_fields<pocca_t> f1;
                f1.insert("1", "1");
                basic_fields<pocca_t> f2;
                f2 = f1;
                BEAST_EXPECT(f2["1"] == "1");
            }
            {
                // propagate_on_container_copy_assignment : false
                using pocca_t = test::test_allocator<char,
                    true, false, true, true, true>;
                basic_fields<pocca_t> f1;
                f1.insert("1", "1");
                basic_fields<pocca_t> f2;
                f2 = f1;
                BEAST_EXPECT(f2["1"] == "1");
            }
        }

        // swap
        {
            {
                // propagate_on_container_swap : true
                using pocs_t = test::test_allocator<char,
                    false, true, true, true, true>;
                pocs_t a1, a2;
                BEAST_EXPECT(a1 != a2);
                basic_fields<pocs_t> f1{a1};
                f1.insert("1", "1");
                basic_fields<pocs_t> f2{a2};
                BEAST_EXPECT(f1.get_allocator() == a1);
                BEAST_EXPECT(f2.get_allocator() == a2);
                swap(f1, f2);
                BEAST_EXPECT(f1.get_allocator() == a2);
                BEAST_EXPECT(f2.get_allocator() == a1);
                BEAST_EXPECT(f1.begin() == f1.end());
                BEAST_EXPECT(f2["1"] == "1");
                swap(f1, f2);
                BEAST_EXPECT(f1.get_allocator() == a1);
                BEAST_EXPECT(f2.get_allocator() == a2);
                BEAST_EXPECT(f1["1"] == "1");
                BEAST_EXPECT(f2.begin() == f2.end());
            }
            {
                // propagate_on_container_swap : false
                using pocs_t = test::test_allocator<char,
                    true, true, true, false, true>;
                pocs_t a1, a2;
                BEAST_EXPECT(a1 == a2);
                BEAST_EXPECT(a1.id() != a2.id());
                basic_fields<pocs_t> f1{a1};
                f1.insert("1", "1");
                basic_fields<pocs_t> f2{a2};
                BEAST_EXPECT(f1.get_allocator() == a1);
                BEAST_EXPECT(f2.get_allocator() == a2);
                swap(f1, f2);
                BEAST_EXPECT(f1.get_allocator().id() == a1.id());
                BEAST_EXPECT(f2.get_allocator().id() == a2.id());
                BEAST_EXPECT(f1.begin() == f1.end());
                BEAST_EXPECT(f2["1"] == "1");
                swap(f1, f2);
                BEAST_EXPECT(f1.get_allocator().id() == a1.id());
                BEAST_EXPECT(f2.get_allocator().id() == a2.id());
                BEAST_EXPECT(f1["1"] == "1");
                BEAST_EXPECT(f2.begin() == f2.end());
            }
        }

        // operations
        {
            fields f;
            f.insert(field::user_agent, "x");
            BEAST_EXPECT(f.count(field::user_agent));
            BEAST_EXPECT(f.count(to_string(field::user_agent)));
            BEAST_EXPECT(f.count(field::user_agent) == 1);
            BEAST_EXPECT(f.count(to_string(field::user_agent)) == 1);
            f.insert(field::user_agent, "y");
            BEAST_EXPECT(f.count(field::user_agent) == 2);
        }
    }

    void testHeaders()
    {
        f_t f1;
        BEAST_EXPECT(empty(f1));
        fill(1, f1);
        BEAST_EXPECT(size(f1) == 1);
        f_t f2;
        f2 = f1;
        BEAST_EXPECT(size(f2) == 1);
        f2.insert("2", "2");
        BEAST_EXPECT(std::distance(f2.begin(), f2.end()) == 2);
        f1 = std::move(f2);
        BEAST_EXPECT(size(f1) == 2);
        BEAST_EXPECT(size(f2) == 0);
        f_t f3(std::move(f1));
        BEAST_EXPECT(size(f3) == 2);
        BEAST_EXPECT(size(f1) == 0);
        self_assign(f3, std::move(f3));
        BEAST_EXPECT(size(f3) == 2);
        BEAST_EXPECT(f2.erase("Not-Present") == 0);
    }

    void testRFC2616()
    {
        f_t f;
        f.insert("a", "w");
        f.insert("a", "x");
        f.insert("aa", "y");
        f.insert("f", "z");
        BEAST_EXPECT(f.count("a") == 2);
    }

    void testErase()
    {
        f_t f;
        f.insert("a", "w");
        f.insert("a", "x");
        f.insert("aa", "y");
        f.insert("f", "z");
        BEAST_EXPECT(size(f) == 4);
        f.erase("a");
        BEAST_EXPECT(size(f) == 2);
    }

    void testIteratorErase()
    {
        f_t f;
        f.insert("a", "x");
        f.insert("b", "y");
        f.insert("c", "z");
        BEAST_EXPECT(size(f) == 3);
        f_t::const_iterator i = std::next(f.begin());
        f.erase(i);
        BEAST_EXPECT(size(f) == 2);
        BEAST_EXPECT(std::next(f.begin(), 0)->name_string() == "a");
        BEAST_EXPECT(std::next(f.begin(), 1)->name_string() == "c");
    }

    void
    testContainer()
    {
        {
            // group fields
            fields f;
            f.insert(field::age,   to_static_string(1));
            f.insert(field::body,  to_static_string(2));
            f.insert(field::close, to_static_string(3));
            f.insert(field::body,  to_static_string(4));
            BEAST_EXPECT(std::next(f.begin(), 0)->name() == field::age);
            BEAST_EXPECT(std::next(f.begin(), 1)->name() == field::body);
            BEAST_EXPECT(std::next(f.begin(), 2)->name() == field::body);
            BEAST_EXPECT(std::next(f.begin(), 3)->name() == field::close);
            BEAST_EXPECT(std::next(f.begin(), 0)->name_string() == "Age");
            BEAST_EXPECT(std::next(f.begin(), 1)->name_string() == "Body");
            BEAST_EXPECT(std::next(f.begin(), 2)->name_string() == "Body");
            BEAST_EXPECT(std::next(f.begin(), 3)->name_string() == "Close");
            BEAST_EXPECT(std::next(f.begin(), 0)->value() == "1");
            BEAST_EXPECT(std::next(f.begin(), 1)->value() == "2");
            BEAST_EXPECT(std::next(f.begin(), 2)->value() == "4");
            BEAST_EXPECT(std::next(f.begin(), 3)->value() == "3");
            BEAST_EXPECT(f.erase(field::body) == 2);
            BEAST_EXPECT(std::next(f.begin(), 0)->name_string() == "Age");
            BEAST_EXPECT(std::next(f.begin(), 1)->name_string() == "Close");
        }
        {
            // group fields, case insensitive
            fields f;
            f.insert("a",  to_static_string(1));
            f.insert("ab", to_static_string(2));
            f.insert("b",  to_static_string(3));
            f.insert("AB", to_static_string(4));
            BEAST_EXPECT(std::next(f.begin(), 0)->name() == field::unknown);
            BEAST_EXPECT(std::next(f.begin(), 1)->name() == field::unknown);
            BEAST_EXPECT(std::next(f.begin(), 2)->name() == field::unknown);
            BEAST_EXPECT(std::next(f.begin(), 3)->name() == field::unknown);
            BEAST_EXPECT(std::next(f.begin(), 0)->name_string() == "a");
            BEAST_EXPECT(std::next(f.begin(), 1)->name_string() == "ab");
            BEAST_EXPECT(std::next(f.begin(), 2)->name_string() == "AB");
            BEAST_EXPECT(std::next(f.begin(), 3)->name_string() == "b");
            BEAST_EXPECT(std::next(f.begin(), 0)->value() == "1");
            BEAST_EXPECT(std::next(f.begin(), 1)->value() == "2");
            BEAST_EXPECT(std::next(f.begin(), 2)->value() == "4");
            BEAST_EXPECT(std::next(f.begin(), 3)->value() == "3");
            BEAST_EXPECT(f.erase("Ab") == 2);
            BEAST_EXPECT(std::next(f.begin(), 0)->name_string() == "a");
            BEAST_EXPECT(std::next(f.begin(), 1)->name_string() == "b");
        }
        {
            // verify insertion orde
            fields f;
            f.insert( "a", to_static_string(1));
            f.insert("dd", to_static_string(2));
            f.insert("b",  to_static_string(3));
            f.insert("dD", to_static_string(4));
            f.insert("c",  to_static_string(5));
            f.insert("Dd", to_static_string(6));
            f.insert("DD", to_static_string(7));
            f.insert( "e", to_static_string(8));
            BEAST_EXPECT(f.count("dd") == 4);
            BEAST_EXPECT(std::next(f.begin(), 1)->name_string() == "dd");
            BEAST_EXPECT(std::next(f.begin(), 2)->name_string() == "dD");
            BEAST_EXPECT(std::next(f.begin(), 3)->name_string() == "Dd");
            BEAST_EXPECT(std::next(f.begin(), 4)->name_string() == "DD");
            f.set("dd", "-");
            BEAST_EXPECT(f.count("dd") == 1);
            BEAST_EXPECT(f["dd"] == "-");
        }

        // equal_range
        {
            fields f;
            f.insert("E", to_static_string(1));
            f.insert("B", to_static_string(2));
            f.insert("D", to_static_string(3));
            f.insert("B", to_static_string(4));
            f.insert("C", to_static_string(5));
            f.insert("B", to_static_string(6));
            f.insert("A", to_static_string(7));
            auto const rng = f.equal_range("B");
            BEAST_EXPECT(std::distance(rng.first, rng.second) == 3);
            BEAST_EXPECT(std::next(rng.first, 0)->value() == "2");
            BEAST_EXPECT(std::next(rng.first, 1)->value() == "4");
            BEAST_EXPECT(std::next(rng.first, 2)->value() == "6");
        }
    }

    struct sized_body
    {
        using value_type = std::uint64_t;

        static
        std::uint64_t
        size(value_type const& v)
        {
            return v;
        }
    };

    struct unsized_body
    {
        struct value_type {};
    };

    void
    testPreparePayload()
    {
        // GET, empty
        {
            request<empty_body> req;
            req.version(11);
            req.method(verb::get);

            req.prepare_payload();
            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req.count(field::transfer_encoding) == 0);

            req.set(field::content_length, "0");
            req.set(field::transfer_encoding, "chunked");
            req.prepare_payload();

            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req.count(field::transfer_encoding) == 0);

            req.set(field::transfer_encoding, "deflate");
            req.prepare_payload();
            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req[field::transfer_encoding] == "deflate");

            req.set(field::transfer_encoding, "deflate, chunked");
            req.prepare_payload();
            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req[field::transfer_encoding] == "deflate");
        }

        // GET, sized
        {
            request<sized_body> req;
            req.version(11);
            req.method(verb::get);
            req.body() = 50;

            req.prepare_payload();
            BEAST_EXPECT(req[field::content_length] == "50");
            BEAST_EXPECT(req[field::transfer_encoding] == "");

            req.set(field::content_length, "0");
            req.set(field::transfer_encoding, "chunked");
            req.prepare_payload();
            BEAST_EXPECT(req[field::content_length] == "50");
            BEAST_EXPECT(req.count(field::transfer_encoding) == 0);

            req.set(field::transfer_encoding, "deflate, chunked");
            req.prepare_payload();
            BEAST_EXPECT(req[field::content_length] == "50");
            BEAST_EXPECT(req[field::transfer_encoding] == "deflate");
        }

        // PUT, empty
        {
            request<empty_body> req;
            req.version(11);
            req.method(verb::put);

            req.prepare_payload();
            BEAST_EXPECT(req[field::content_length] == "0");
            BEAST_EXPECT(req.count(field::transfer_encoding) == 0);

            req.set(field::content_length, "50");
            req.set(field::transfer_encoding, "deflate, chunked");
            req.prepare_payload();
            BEAST_EXPECT(req[field::content_length] == "0");
            BEAST_EXPECT(req[field::transfer_encoding] == "deflate");
        }

        // PUT, sized
        {
            request<sized_body> req;
            req.version(11);
            req.method(verb::put);
            req.body() = 50;

            req.prepare_payload();
            BEAST_EXPECT(req[field::content_length] == "50");
            BEAST_EXPECT(req.count(field::transfer_encoding) == 0);

            req.set(field::content_length, "25");
            req.set(field::transfer_encoding, "deflate, chunked");
            req.prepare_payload();
            BEAST_EXPECT(req[field::content_length] == "50");
            BEAST_EXPECT(req[field::transfer_encoding] == "deflate");
        }

        // POST, unsized
        {
            request<unsized_body> req;
            req.version(11);
            req.method(verb::post);

            req.prepare_payload();
            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req[field::transfer_encoding] == "chunked");

            req.set(field::transfer_encoding, "deflate");
            req.prepare_payload();
            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req[field::transfer_encoding] == "deflate, chunked");
        }

        // POST, unsized HTTP/1.0
        {
            request<unsized_body> req;
            req.version(10);
            req.method(verb::post);

            req.prepare_payload();
            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req.count(field::transfer_encoding) == 0);

            req.set(field::transfer_encoding, "deflate");
            req.prepare_payload();
            BEAST_EXPECT(req.count(field::content_length) == 0);
            BEAST_EXPECT(req[field::transfer_encoding] == "deflate");
        }

        // OK, empty
        {
            response<empty_body> res;
            res.version(11);

            res.prepare_payload();
            BEAST_EXPECT(res[field::content_length] == "0");
            BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

            res.erase(field::content_length);
            res.set(field::transfer_encoding, "chunked");
            res.prepare_payload();
            BEAST_EXPECT(res[field::content_length] == "0");
            BEAST_EXPECT(res.count(field::transfer_encoding) == 0);
        }

        // OK, sized
        {
            response<sized_body> res;
            res.version(11);
            res.body() = 50;

            res.prepare_payload();
            BEAST_EXPECT(res[field::content_length] == "50");
            BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

            res.erase(field::content_length);
            res.set(field::transfer_encoding, "chunked");
            res.prepare_payload();
            BEAST_EXPECT(res[field::content_length] == "50");
            BEAST_EXPECT(res.count(field::transfer_encoding) == 0);
        }

        // OK, unsized
        {
            response<unsized_body> res;
            res.version(11);

            res.prepare_payload();
            BEAST_EXPECT(res.count(field::content_length) == 0);
            BEAST_EXPECT(res[field::transfer_encoding] == "chunked");
        }
    }

    void
    testKeepAlive()
    {
        response<empty_body> res;
        auto const keep_alive =
            [&](bool v)
            {
                res.keep_alive(v);
                BEAST_EXPECT(
                    (res.keep_alive() && v) ||
                    (! res.keep_alive() && ! v));
            };

        std::string const big(max_static_buffer + 1, 'a');

        // HTTP/1.0
        res.version(10);
        res.erase(field::connection);

        keep_alive(false);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.set(field::connection, "close");
        keep_alive(false);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.set(field::connection, "keep-alive");
        keep_alive(false);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.set(field::connection, "keep-alive, close");
        keep_alive(false);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.erase(field::connection);
        keep_alive(true);
        BEAST_EXPECT(res[field::connection] == "keep-alive");

        res.set(field::connection, "close");
        keep_alive(true);
        BEAST_EXPECT(res[field::connection] == "keep-alive");

        res.set(field::connection, "keep-alive");
        keep_alive(true);
        BEAST_EXPECT(res[field::connection] == "keep-alive");

        res.set(field::connection, "keep-alive, close");
        keep_alive(true);
        BEAST_EXPECT(res[field::connection] == "keep-alive");

        auto const test10 =
            [&](std::string s)
        {
            res.set(field::connection, s);
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, s + ", close");
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, "keep-alive, " + s);
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, "keep-alive, " + s + ", close");
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, s);
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == s + ", keep-alive");

            res.set(field::connection, s + ", close");
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == s + ", keep-alive");

            res.set(field::connection, "keep-alive, " + s);
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == "keep-alive, " + s);

            res.set(field::connection, "keep-alive, " + s+ ", close");
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == "keep-alive, " + s);
        };

        test10("foo");
        test10(big);

        // HTTP/1.1
        res.version(11);

        res.erase(field::connection);
        keep_alive(true);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.set(field::connection, "close");
        keep_alive(true);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.set(field::connection, "keep-alive");
        keep_alive(true);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.set(field::connection, "keep-alive, close");
        keep_alive(true);
        BEAST_EXPECT(res.count(field::connection) == 0);

        res.erase(field::connection);
        keep_alive(false);
        BEAST_EXPECT(res[field::connection] == "close");

        res.set(field::connection, "close");
        keep_alive(false);
        BEAST_EXPECT(res[field::connection] == "close");

        res.set(field::connection, "keep-alive");
        keep_alive(false);
        BEAST_EXPECT(res[field::connection] == "close");

        res.set(field::connection, "keep-alive, close");
        keep_alive(false);
        BEAST_EXPECT(res[field::connection] == "close");

        auto const test11 =
            [&](std::string s)
        {
            res.set(field::connection, s);
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, s + ", close");
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, "keep-alive, " + s);
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, "keep-alive, " + s + ", close");
            keep_alive(true);
            BEAST_EXPECT(res[field::connection] == s);

            res.set(field::connection, s);
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == s + ", close");

            res.set(field::connection, "close, " + s);
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == "close, " + s);

            res.set(field::connection, "keep-alive, " + s);
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == s + ", close");

            res.set(field::connection, "close, " + s + ", keep-alive");
            keep_alive(false);
            BEAST_EXPECT(res[field::connection] == "close, " + s);
        };

        test11("foo");
        test11(big);
    }

    void
    testContentLength()
    {
        response<empty_body> res{status::ok, 11};
        BEAST_EXPECT(res.count(field::content_length) == 0);
        BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

        res.content_length(0);
        BEAST_EXPECT(res[field::content_length] == "0");

        res.content_length(100);
        BEAST_EXPECT(res[field::content_length] == "100");

        res.content_length(boost::none);
        BEAST_EXPECT(res.count(field::content_length) == 0);

        res.set(field::transfer_encoding, "chunked");
        res.content_length(0);
        BEAST_EXPECT(res[field::content_length] == "0");
        BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

        res.set(field::transfer_encoding, "chunked");
        res.content_length(100);
        BEAST_EXPECT(res[field::content_length] == "100");
        BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

        res.set(field::transfer_encoding, "chunked");
        res.content_length(boost::none);
        BEAST_EXPECT(res.count(field::content_length) == 0);
        BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

        auto const check = [&](std::string s)
        {
            res.set(field::transfer_encoding, s);
            res.content_length(0);
            BEAST_EXPECT(res[field::content_length] == "0");
            BEAST_EXPECT(res[field::transfer_encoding] == s);

            res.set(field::transfer_encoding, s);
            res.content_length(100);
            BEAST_EXPECT(res[field::content_length] == "100");
            BEAST_EXPECT(res[field::transfer_encoding] == s);

            res.set(field::transfer_encoding, s);
            res.content_length(boost::none);
            BEAST_EXPECT(res.count(field::content_length) == 0);
            BEAST_EXPECT(res[field::transfer_encoding] == s);

            res.set(field::transfer_encoding, s + ", chunked");
            res.content_length(0);
            BEAST_EXPECT(res[field::content_length] == "0");
            BEAST_EXPECT(res[field::transfer_encoding] == s);

            res.set(field::transfer_encoding, s + ", chunked");
            res.content_length(100);
            BEAST_EXPECT(res[field::content_length] == "100");
            BEAST_EXPECT(res[field::transfer_encoding] == s);

            res.set(field::transfer_encoding, s + ", chunked");
            res.content_length(boost::none);
            BEAST_EXPECT(res.count(field::content_length) == 0);
            BEAST_EXPECT(res[field::transfer_encoding] == s);

            res.set(field::transfer_encoding, "chunked, " + s);
            res.content_length(0);
            BEAST_EXPECT(res[field::content_length] == "0");
            BEAST_EXPECT(res[field::transfer_encoding] == "chunked, " + s);

            res.set(field::transfer_encoding, "chunked, " + s);
            res.content_length(100);
            BEAST_EXPECT(res[field::content_length] == "100");
            BEAST_EXPECT(res[field::transfer_encoding] == "chunked, " + s);

            res.set(field::transfer_encoding, "chunked, " + s);
            res.content_length(boost::none);
            BEAST_EXPECT(res.count(field::content_length) == 0);
            BEAST_EXPECT(res[field::transfer_encoding] == "chunked, " + s);
        };

        check("foo");

        std::string const big(max_static_buffer + 1, 'a');

        check(big);
    }

    void
    testChunked()
    {
        response<empty_body> res{status::ok, 11};
        BEAST_EXPECT(res.count(field::content_length) == 0);
        BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

        auto const chunked =
            [&](bool v)
            {
                res.chunked(v);
                BEAST_EXPECT(
                    (res.chunked() && v) ||
                    (! res.chunked() && ! v));
                BEAST_EXPECT(res.count(
                    field::content_length) == 0);
            };

        res.erase(field::transfer_encoding);
        res.set(field::content_length, to_static_string(32));
        chunked(true);
        BEAST_EXPECT(res[field::transfer_encoding] == "chunked");

        res.set(field::transfer_encoding, "chunked");
        chunked(true);
        BEAST_EXPECT(res[field::transfer_encoding] == "chunked");

        res.erase(field::transfer_encoding);
        res.set(field::content_length, to_static_string(32));
        chunked(false);
        BEAST_EXPECT(res.count(field::transfer_encoding) == 0);

        res.set(field::transfer_encoding, "chunked");
        chunked(false);
        BEAST_EXPECT(res.count(field::transfer_encoding) == 0);



        res.set(field::transfer_encoding, "foo");
        chunked(true);
        BEAST_EXPECT(res[field::transfer_encoding] == "foo, chunked");

        res.set(field::transfer_encoding, "chunked, foo");
        chunked(true);
        BEAST_EXPECT(res[field::transfer_encoding] == "chunked, foo, chunked");

        res.set(field::transfer_encoding, "chunked, foo, chunked");
        chunked(true);
        BEAST_EXPECT(res[field::transfer_encoding] == "chunked, foo, chunked");

        res.set(field::transfer_encoding, "foo, chunked");
        chunked(false);
        BEAST_EXPECT(res[field::transfer_encoding] == "foo");

        res.set(field::transfer_encoding, "chunked, foo");
        chunked(false);
        BEAST_EXPECT(res[field::transfer_encoding] == "chunked, foo");

        res.set(field::transfer_encoding, "chunked, foo, chunked");
        chunked(false);
        BEAST_EXPECT(res[field::transfer_encoding] == "chunked, foo");
    }

    void
    testIssue1828()
    {
        beast::http::fields req;
        req.insert("abc", "1");
        req.insert("abc", "2");
        req.insert("abc", "3");
        BEAST_EXPECT(req.count("abc") == 3);
        auto iter = req.find("abc");
        BEAST_EXPECT(iter->value() == "1");
        req.insert("abc", "4");
        req.erase(iter);
        BEAST_EXPECT(req.count("abc") == 3);
    }

    template<class Arg1, class InArg>
    struct set_test
    {
        static auto test(...) ->
            std::false_type;

        template<class U = InArg>
        static auto test(U arg) ->
            decltype(std::declval<fields>().
                set(std::declval<Arg1>(),
                    std::declval<U>()),
                std::true_type());

        static constexpr bool value =
            decltype(test(std::declval<InArg>()))::value;
    };

    template<class Arg1, class InArg>
    struct insert_test
    {
        static auto test(...) ->
            std::false_type;

        template<class U = InArg>
        static auto test(U arg) ->
            decltype(std::declval<fields>().
                insert(std::declval<Arg1>(),
                    std::declval<U>()),
                std::true_type());

        static constexpr bool value =
            decltype(test(std::declval<InArg>()))::value;
    };

    void
    testIssue2085()
    {
        BOOST_STATIC_ASSERT((! set_test<field, int>::value));
        BOOST_STATIC_ASSERT((! set_test<field, std::nullptr_t>::value));
        BOOST_STATIC_ASSERT((! set_test<field, double>::value));
        BOOST_STATIC_ASSERT((! set_test<string_view, int>::value));
        BOOST_STATIC_ASSERT((! set_test<string_view, std::nullptr_t>::value));
        BOOST_STATIC_ASSERT((! set_test<string_view, double>::value));

        BOOST_STATIC_ASSERT(( set_test<field, const char*>::value));
        BOOST_STATIC_ASSERT(( set_test<field, string_view>::value));
        BOOST_STATIC_ASSERT(( set_test<field, const char(&)[10]>::value));
        BOOST_STATIC_ASSERT(( set_test<string_view, const char*>::value));
        BOOST_STATIC_ASSERT(( set_test<string_view, string_view>::value));
        BOOST_STATIC_ASSERT(( set_test<string_view, const char(&)[10]>::value));

        BOOST_STATIC_ASSERT((! insert_test<field, int>::value));
        BOOST_STATIC_ASSERT((! insert_test<field, std::nullptr_t>::value));
        BOOST_STATIC_ASSERT((! insert_test<field, double>::value));
        BOOST_STATIC_ASSERT((! insert_test<string_view, int>::value));
        BOOST_STATIC_ASSERT((! insert_test<string_view, std::nullptr_t>::value));
        BOOST_STATIC_ASSERT((! insert_test<string_view, double>::value));

        BOOST_STATIC_ASSERT(( insert_test<field, const char*>::value));
        BOOST_STATIC_ASSERT(( insert_test<field, string_view>::value));
        BOOST_STATIC_ASSERT(( insert_test<field, const char(&)[10]>::value));
        BOOST_STATIC_ASSERT(( insert_test<string_view, const char*>::value));
        BOOST_STATIC_ASSERT(( insert_test<string_view, string_view>::value));
        BOOST_STATIC_ASSERT(( insert_test<string_view, const char(&)[10]>::value));
    }

    void
    run() override
    {
        testMembers();
        testHeaders();
        testRFC2616();
        testErase();
        testIteratorErase();
        testContainer();
        testPreparePayload();

        testKeepAlive();
        testContentLength();
        testChunked();

        testIssue1828();
        boost::ignore_unused(&fields_test::testIssue2085);
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,fields);

} // http
} // beast
} // boost
