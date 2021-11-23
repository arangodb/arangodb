//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/parser.hpp>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class parser_test
{
public:
    void
    testCtors()
    {
        // parser(parser const&)
        BOOST_STATIC_ASSERT(
            ! std::is_copy_constructible<parser>::value);

        // operator=(parser const&)
        BOOST_STATIC_ASSERT(
            ! std::is_copy_assignable<parser>::value);

        // ~parser()
        {
            // implied
        }

        // parser(storage_ptr, parse_options, unsigned char*, size_t)
        {
            unsigned char buf[256];
            parser p(
                storage_ptr(),
                parse_options(),
                &buf[0],
                sizeof(buf));
        }

        // parser()
        {
            parser p;
        }

        // parser(storage_ptr, parse_options)
        {
            parser p(storage_ptr{}, parse_options());
        }

        // parser(storage_ptr)
        {
            parser p(storage_ptr{});
        }

        // parser(storage_ptr, parse_options, unsigned char[])
        {
            unsigned char buf[256];
            parser p(
                storage_ptr(),
                parse_options(),
                buf);
        }

    #if defined(__cpp_lib_byte)
        // parser(storage_ptr, parse_options, std::byte*, size_t)
        {
            std::byte buf[256];
            parser p(
                storage_ptr(),
                parse_options(),
                &buf[0],
                sizeof(buf));
        }

        // parser(storage_ptr, parse_options, std::byte[])
        {
            std::byte buf[256];
            parser p(
                storage_ptr(),
                parse_options(),
                buf);
        }
    #endif

        // parser(storage_ptr, parse_options, unsigned char[], size_t)
        {
            unsigned char buf[256];
            parser p(
                storage_ptr(),
                parse_options(),
                buf,
                sizeof(buf));
        }

    #if defined(__cpp_lib_byte)
        // parser(storage_ptr, parse_options, std::byte[], size_t)
        {
            std::byte buf[256];
            parser p(
                storage_ptr(),
                parse_options(),
                buf,
                sizeof(buf));
        }
    #endif
    }

    void
    testMembers()
    {
        // reset
        {
            parser p;
            p.reset();
        }

        // write_some(char const*, size_t, error_code&)
        {
            // valid json
            {
                parser p;
                error_code ec;
                auto const n =
                    p.write_some("null", 4, ec);
                BOOST_TEST(! ec);
                BOOST_TEST(n == 4);
            }

            // valid json with trailing space
            {
                parser p;
                error_code ec;
                auto const n =
                    p.write_some("null ", 5, ec);
                BOOST_TEST(! ec);
                BOOST_TEST(n == 5);
            }

            // valid json with invalid trailing char
            {
                parser p;
                error_code ec;
                auto const n =
                    p.write_some("null*", 5, ec);
                BOOST_TEST(! ec);
                BOOST_TEST(n == 4);
            }

            // partial json
            {
                parser p;
                error_code ec;
                p.write_some("nul", 3, ec);
                BOOST_TEST(ec);
            }
        }

        // write_some(string_view, error_code&)
        {
            // valid json
            {
                parser p;
                error_code ec;
                auto const n =
                    p.write_some("null", ec);
                BOOST_TEST(! ec);
                BOOST_TEST(n == 4);
            }

            // partial json
            {
                parser p;
                error_code ec;
                p.write_some("nul", ec);
                BOOST_TEST(ec);
            }
        }

        // write_some(char const*, size_t)
        {
            // valid json with trailing space
            {
                parser p;
                auto const n =
                    p.write_some("null ", 5);
                BOOST_TEST(n == 5);
            }

            // partial json
            {
                parser p;
                BOOST_TEST_THROWS(
                    p.write_some("nul", 3),
                    system_error);
            }
        }

        // write_some(string_view)
        {
            // valid json with trailing space
            {
                parser p;
                auto const n =
                    p.write_some("null ");
                BOOST_TEST(n == 5);
            }

            // partial json
            {
                parser p;
                BOOST_TEST_THROWS(
                    p.write_some("nul"),
                    system_error);
            }
        }

        //--------------------------------------------------

        // write(char const*, size_t, error_code&)
        {
            // valid json
            {
                parser p;
                error_code ec;
                auto const n =
                    p.write("null", 4, ec);
                BOOST_TEST(! ec);
                BOOST_TEST(n == 4);
            }

            // valid json with trailing space
            {
                parser p;
                error_code ec;
                auto const n =
                    p.write("null ", 5, ec);
                BOOST_TEST(! ec);
                BOOST_TEST(n == 5);
            }

            // valid json with invalid trailing char
            {
                parser p;
                error_code ec;
                p.write("null*", 5, ec);
                BOOST_TEST(ec);
            }

            // partial json
            {
                parser p;
                error_code ec;
                p.write("nul", 3, ec);
                BOOST_TEST(ec);
            }
        }

        // write(string_view, error_code&)
        {
            // valid json
            {
                parser p;
                error_code ec;
                auto const n =
                    p.write("null", ec);
                BOOST_TEST(! ec);
                BOOST_TEST(n == 4);
            }

            // partial json
            {
                parser p;
                error_code ec;
                p.write("nul", ec);
                BOOST_TEST(ec);
            }
        }

        // write(char const*, size_t)
        {
            // valid json with trailing space
            {
                parser p;
                auto const n =
                    p.write("null ", 5);
                BOOST_TEST(n == 5);
            }

            // valid json with invalid trailing char
            {
                parser p;
                BOOST_TEST_THROWS(
                    p.write("null*", 5),
                    system_error);
            }

            // partial json
            {
                parser p;
                BOOST_TEST_THROWS(
                    p.write("nul", 3),
                    system_error);
            }
        }

        // write(string_view)
        {
            // valid json with trailing space
            {
                parser p;
                auto const n =
                    p.write("null ");
                BOOST_TEST(n == 5);
            }

            // valid json with invalid trailing char
            {
                parser p;
                BOOST_TEST_THROWS(
                    p.write("null*"),
                    system_error);
            }

            // partial json
            {
                parser p;
                BOOST_TEST_THROWS(
                    p.write("nul"),
                    system_error);
            }
        }

        // release
        {
            // valid json
            {
                parser p;
                p.write("[1,2,3]");
                BOOST_TEST(p.release() ==
                    value({1,2,3}));
            }

            // release with no write
            {
                parser p;
                BOOST_TEST_THROWS(
                    p.release(),
                    system_error);
            }

            // release after error
            {
                parser p;
                error_code ec;
                p.write("nul", ec);
                BOOST_TEST(ec);
                BOOST_TEST_THROWS(
                    p.release(),
                    system_error);
            }
        }
    }

    void
    run()
    {
        testCtors();
        testMembers();
    }
};

TEST_SUITE(parser_test, "boost.json.parser");

BOOST_JSON_NS_END
