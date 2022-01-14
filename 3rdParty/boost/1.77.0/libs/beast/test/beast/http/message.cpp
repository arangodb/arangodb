//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/message.hpp>

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace http {

class message_test : public beast::unit_test::suite
{
public:
    struct Arg1
    {
        bool moved = false;

        Arg1(Arg1 const&) = default;

        Arg1()
        {
        }

        Arg1(Arg1&& other)
        {
            other.moved = true;
        }
    };

    struct Arg2 { };
    struct Arg3 { };

    // default constructible Body
    struct default_body
    {
        using value_type = std::string;
    };

    // 1-arg constructible Body
    struct one_arg_body
    {
        struct value_type
        {
            explicit
            value_type(Arg1 const&)
            {
            }

            explicit
            value_type(Arg1&& arg)
            {
                Arg1 arg_(std::move(arg));
            }
        };
    };

    // 2-arg constructible Body
    struct two_arg_body
    {
        struct value_type
        {
            value_type(Arg1 const&, Arg2 const&)
            {
            }
        };
    };

    // 0-arg
    BOOST_STATIC_ASSERT(std::is_constructible<
        request<default_body>>::value);

    // 1-arg
    BOOST_STATIC_ASSERT(! std::is_constructible<request<one_arg_body>
        >::value);

    //BOOST_STATIC_ASSERT(! std::is_constructible<request<one_arg_body>,
    //    verb, string_view, unsigned>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<request<one_arg_body>,
        verb, string_view, unsigned, Arg1>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<request<one_arg_body>,
        verb, string_view, unsigned, Arg1&&>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<request<one_arg_body>,
        verb, string_view, unsigned, Arg1 const>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<request<one_arg_body>,
        verb, string_view, unsigned, Arg1 const&>::value);

    // 1-arg + fields
    BOOST_STATIC_ASSERT(std::is_constructible<request<one_arg_body>,
        verb, string_view, unsigned, Arg1, fields::allocator_type>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<request<one_arg_body>, std::piecewise_construct_t,
            std::tuple<Arg1>>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<request<two_arg_body>, std::piecewise_construct_t,
            std::tuple<Arg1, Arg2>>::value);

    BOOST_STATIC_ASSERT(std::is_constructible<request<two_arg_body>, std::piecewise_construct_t,
            std::tuple<Arg1, Arg2>, std::tuple<fields::allocator_type>>::value);

    // special members
    BOOST_STATIC_ASSERT(std::is_copy_constructible<header<true>>::value);
    BOOST_STATIC_ASSERT(std::is_move_constructible<header<true>>::value);
    BOOST_STATIC_ASSERT(std::is_copy_assignable<header<true>>::value);
    BOOST_STATIC_ASSERT(std::is_move_assignable<header<true>>::value);
    BOOST_STATIC_ASSERT(std::is_copy_constructible<header<false>>::value);
    BOOST_STATIC_ASSERT(std::is_move_constructible<header<false>>::value);
    BOOST_STATIC_ASSERT(std::is_copy_assignable<header<false>>::value);
    BOOST_STATIC_ASSERT(std::is_move_assignable<header<false>>::value);

    void
    testMessage()
    {
        {
            Arg1 arg1;
            request<one_arg_body>{verb::get, "/", 11, std::move(arg1)};
            BEAST_EXPECT(arg1.moved);
        }

        {
            header<true> h;
            h.set(field::user_agent, "test");
            BEAST_EXPECT(h[field::user_agent] == "test");
            request<default_body> m{std::move(h)};
            BEAST_EXPECT(m[field::user_agent] == "test");
            BEAST_EXPECT(h.count(field::user_agent) == 0);
        }
        {
            request<empty_body> h{verb::get, "/", 10};
            h.set(field::user_agent, "test");
            request<one_arg_body> m{std::move(h.base()), Arg1{}};
            BEAST_EXPECT(m["User-Agent"] == "test");
            BEAST_EXPECT(h.count(http::field::user_agent) == 0);
            BEAST_EXPECT(m.method() == verb::get);
            BEAST_EXPECT(m.target() == "/");
            BEAST_EXPECT(m.version() == 10);
        }

        // swap
        request<string_body> m1;
        request<string_body> m2;
        m1.target("u");
        m1.body() = "1";
        m1.insert("h", "v");
        m2.method_string("G");
        m2.body() = "2";
        swap(m1, m2);
        BEAST_EXPECT(m1.method_string() == "G");
        BEAST_EXPECT(m2.method_string().empty());
        BEAST_EXPECT(m1.target().empty());
        BEAST_EXPECT(m2.target() == "u");
        BEAST_EXPECT(m1.body() == "2");
        BEAST_EXPECT(m2.body() == "1");
        BEAST_EXPECT(! m1.count("h"));
        BEAST_EXPECT(m2.count("h"));
    }

    struct MoveFields : fields
    {
        bool moved_to = false;
        bool moved_from = false;

        MoveFields() = default;

        MoveFields(MoveFields&& other)
            : moved_to(true)
        {
            other.moved_from = true;
        }

        MoveFields& operator=(MoveFields&&)
        {
            return *this;
        }
    };

    struct token {};

    struct test_fields
    {
        std::string target;

        test_fields() = delete;
        test_fields(token) {}
        string_view get_method_impl() const { return {}; }
        string_view get_target_impl() const { return target; }
        string_view get_reason_impl() const { return {}; }
        bool get_chunked_impl() const { return false; }
        bool get_keep_alive_impl(unsigned) const { return true; }
        bool has_content_length_impl() const { return false; }
        void set_method_impl(string_view) {}
        void set_target_impl(string_view s) { target = std::string(s); }
        void set_reason_impl(string_view) {}
        void set_chunked_impl(bool) {}
        void set_content_length_impl(boost::optional<std::uint64_t>) {}
        void set_keep_alive_impl(unsigned, bool) {}
    };

    void
    testMessageCtors()
    {
        {
            request<empty_body> req;
            BEAST_EXPECT(req.version() == 11);
            BEAST_EXPECT(req.method() == verb::unknown);
            BEAST_EXPECT(req.target() == "");
        }
        {
            request<empty_body> req{verb::get, "/", 11};
            BEAST_EXPECT(req.version() == 11);
            BEAST_EXPECT(req.method() == verb::get);
            BEAST_EXPECT(req.target() == "/");
        }
        {
            request<string_body> req{verb::get, "/", 11, "Hello"};
            BEAST_EXPECT(req.version() == 11);
            BEAST_EXPECT(req.method() == verb::get);
            BEAST_EXPECT(req.target() == "/");
            BEAST_EXPECT(req.body() == "Hello");
        }
        {
            request<string_body, test_fields> req{
                verb::get, "/", 11, "Hello", token{}};
            BEAST_EXPECT(req.version() == 11);
            BEAST_EXPECT(req.method() == verb::get);
            BEAST_EXPECT(req.target() == "/");
            BEAST_EXPECT(req.body() == "Hello");
            BEAST_EXPECT(! req.has_content_length());
        }
        {
            response<string_body> res;
            BEAST_EXPECT(res.version() == 11);
            BEAST_EXPECT(res.result() == status::ok);
            BEAST_EXPECT(res.reason() == "OK");
        }
        {
            response<string_body> res{status::bad_request, 10};
            BEAST_EXPECT(res.version() == 10);
            BEAST_EXPECT(res.result() == status::bad_request);
            BEAST_EXPECT(res.reason() == "Bad Request");
        }
        {
            response<string_body> res{status::bad_request, 10, "Hello"};
            BEAST_EXPECT(res.version() == 10);
            BEAST_EXPECT(res.result() == status::bad_request);
            BEAST_EXPECT(res.reason() == "Bad Request");
            BEAST_EXPECT(res.body() == "Hello");
        }
        {
            response<string_body, test_fields> res{
                status::bad_request, 10, "Hello", token{}};
            BEAST_EXPECT(res.version() == 10);
            BEAST_EXPECT(res.result() == status::bad_request);
            BEAST_EXPECT(res.reason() == "Bad Request");
            BEAST_EXPECT(res.body() == "Hello");
            BEAST_EXPECT(! res.has_content_length());
        }
    }

    void
    testBody()
    {
        {
            auto f = [](empty_body::value_type&){};
            request<empty_body> m;
            f(m.body());
        }
        {
            auto f = [](empty_body::value_type const&){};
            request<empty_body> const m{};
            f(m.body());
            f(std::move(m.body()));
        }
        {
            auto f = [](empty_body::value_type&&){};
            request<empty_body> m;
            f(std::move(m).body());
            f(std::move(m.body()));
        }
        {
            auto f = [](empty_body::value_type const&&){};
            request<empty_body> const m{};
            f(std::move(m.body()));
        }
    }

    void
    testSwap()
    {
        response<string_body> m1;
        response<string_body> m2;
        m1.result(status::ok);
        m1.version(10);
        m1.body() = "1";
        m1.insert("h", "v");
        m2.result(status::not_found);
        m2.body() = "2";
        m2.version(11);
        swap(m1, m2);
        BEAST_EXPECT(m1.result() == status::not_found);
        BEAST_EXPECT(m1.result_int() == 404);
        BEAST_EXPECT(m2.result() == status::ok);
        BEAST_EXPECT(m2.result_int() == 200);
        BEAST_EXPECT(m1.reason() == "Not Found");
        BEAST_EXPECT(m2.reason() == "OK");
        BEAST_EXPECT(m1.version() == 11);
        BEAST_EXPECT(m2.version() == 10);
        BEAST_EXPECT(m1.body() == "2");
        BEAST_EXPECT(m2.body() == "1");
        BEAST_EXPECT(! m1.count("h"));
        BEAST_EXPECT(m2.count("h"));
    }

    void
    testSpecialMembers()
    {
        response<string_body> r1;
        response<string_body> r2{r1};
        response<string_body> r3{std::move(r2)};
        r2 = r3;
        r1 = std::move(r2);
        [r1]()
        {
        }();
    }

    void
    testMethod()
    {
        header<true> h;
        auto const vcheck =
            [&](verb v)
            {
                h.method(v);
                BEAST_EXPECT(h.method() == v);
                BEAST_EXPECT(h.method_string() == to_string(v));
            };
        auto const scheck =
            [&](string_view s)
            {
                h.method_string(s);
                BEAST_EXPECT(h.method() == string_to_verb(s));
                BEAST_EXPECT(h.method_string() == s);
            };
        vcheck(verb::get);
        vcheck(verb::head);
        scheck("GET");
        scheck("HEAD");
        scheck("XYZ");
    }

    void
    testStatus()
    {
        header<false> h;
        h.result(200);
        BEAST_EXPECT(h.result_int() == 200);
        BEAST_EXPECT(h.result() == status::ok);
        h.result(status::switching_protocols);
        BEAST_EXPECT(h.result_int() == 101);
        BEAST_EXPECT(h.result() == status::switching_protocols);
        h.result(1);
        BEAST_EXPECT(h.result_int() == 1);
        BEAST_EXPECT(h.result() == status::unknown);
    }

    void
    testReason()
    {
        header<false> h;
        h.result(status::ok);
        BEAST_EXPECT(h.reason() == "OK");
        h.reason("Pepe");
        BEAST_EXPECT(h.reason() == "Pepe");
        h.result(status::not_found);
        BEAST_EXPECT(h.reason() == "Pepe");
        h.reason({});
        BEAST_EXPECT(h.reason() == "Not Found");
    }

    void
    testNeedEof()
    {
        {
            request<empty_body> m;
            
            m.version(10);
            m.keep_alive(false);
            BEAST_EXPECT(m.need_eof());
            m.keep_alive(true);
            BEAST_EXPECT(! m.need_eof());

            m.version(11);
            m.keep_alive(false);
            BEAST_EXPECT(m.need_eof());
            m.keep_alive(true);
            BEAST_EXPECT(! m.need_eof());
        }

        {
            response<empty_body> m;
            m.result(status::ok);

            m.version(10);
            m.keep_alive(false);
            BEAST_EXPECT(! m.has_content_length());
            BEAST_EXPECT(! m.chunked());
            m.result(status::no_content);
            BEAST_EXPECT(m.need_eof());
            m.result(status::not_modified);
            BEAST_EXPECT(m.need_eof());
            m.result(status::continue_);
            BEAST_EXPECT(m.need_eof());
            m.result(status::switching_protocols);
            BEAST_EXPECT(m.need_eof());
            m.result(status::ok);
            BEAST_EXPECT(m.need_eof());

            m.version(10);
            m.keep_alive(true);
            BEAST_EXPECT(! m.has_content_length());
            BEAST_EXPECT(! m.chunked());
            m.result(status::no_content);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::not_modified);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::continue_);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::switching_protocols);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::ok);
            BEAST_EXPECT(m.need_eof());
            m.set(field::content_length, "1");
            BEAST_EXPECT(! m.need_eof());
        }

        {
            response<empty_body> m;
            m.result(status::ok);

            m.version(11);
            m.keep_alive(false);
            BEAST_EXPECT(! m.has_content_length());
            BEAST_EXPECT(! m.chunked());
            m.result(status::no_content);
            BEAST_EXPECT(m.need_eof());
            m.result(status::not_modified);
            BEAST_EXPECT(m.need_eof());
            m.result(status::continue_);
            BEAST_EXPECT(m.need_eof());
            m.result(status::switching_protocols);
            BEAST_EXPECT(m.need_eof());
            m.result(status::ok);
            BEAST_EXPECT(m.need_eof());
            m.chunked(true);
            BEAST_EXPECT(m.need_eof());
            m.content_length(1);
            BEAST_EXPECT(m.need_eof());
        }

        {
            response<empty_body> m;
            m.result(status::ok);

            m.version(11);
            m.keep_alive(true);
            BEAST_EXPECT(! m.has_content_length());
            BEAST_EXPECT(! m.chunked());
            m.result(status::no_content);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::not_modified);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::continue_);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::switching_protocols);
            BEAST_EXPECT(! m.need_eof());
            m.result(status::ok);
            BEAST_EXPECT(m.need_eof());
            m.chunked(true);
            BEAST_EXPECT(! m.need_eof());
            m.content_length(1);
            BEAST_EXPECT(! m.need_eof());
        }
    }

    void
    run() override
    {
        testMessage();
        testMessageCtors();
        testBody();
        testSwap();
        testSpecialMembers();
        testMethod();
        testStatus();
        testReason();
        testNeedEof();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,message);

} // http
} // beast
} // boost
