//
// Copyright (c) 2020 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/static_resource.hpp>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <iostream>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

BOOST_STATIC_ASSERT( std::is_nothrow_destructible<static_resource>::value );

class static_resource_test
{
public:
    void
    testJavadocs()
    {
    //--------------------------------------

    unsigned char buf[ 4000 ];
    static_resource mr( buf );

    // Parse the string, using our memory resource
    value jv = parse( "[1,2,3]", &mr );

    // Print the JSON
    std::cout << jv;

    //--------------------------------------
    }

    void
    test()
    {
        // static_resource(unsigned char*, size_t)
        {
            unsigned char buf[1000];
            static_resource mr(
                &buf[0], sizeof(buf));
            BOOST_TEST(serialize(parse(
                "[1,2,3]", &mr)) == "[1,2,3]");
        }

    #if defined(__cpp_lib_byte)
        // static_resource(std::byte*, size_t)
        {
            std::byte buf[1000];
            static_resource mr(
                &buf[0], sizeof(buf));
            BOOST_TEST(serialize(parse(
                "[1,2,3]", &mr)) == "[1,2,3]");
        }
    #endif

        // static_resource(unsigned char[N])
        {
            unsigned char buf[10];
            static_resource mr(buf);
            BOOST_TEST_THROWS(
                serialize(parse("[1,2,3]", &mr)),
                std::bad_alloc);
        }

    #if defined(__cpp_lib_byte)
        // static_resource(std::byte[N])
        {
            std::byte buf[10];
            static_resource mr(buf);
            BOOST_TEST_THROWS(
                serialize(parse("[1,2,3]", &mr)),
                std::bad_alloc);
        }
    #endif

        // static_resource(unsigned char[N], size_t)
        {
            unsigned char buf[1000];
            static_resource mr(
                buf, 500);
            BOOST_TEST(serialize(parse(
                "[1,2,3]", &mr)) == "[1,2,3]");
        }

    #if defined(__cpp_lib_byte)
        // static_resource(std::byte[N])
        {
            std::byte buf[1000];
            static_resource mr(
                buf, 500);
            BOOST_TEST(serialize(parse(
                "[1,2,3]", &mr)) == "[1,2,3]");
        }
    #endif

        // release()
        {
            unsigned char buf[10];
            static_resource mr(
                buf, sizeof(buf));
            (void)mr.allocate(10,1);
            BOOST_TEST_THROWS(
                mr.allocate(10,1),
                std::bad_alloc);
            mr.release();
            (void)mr.allocate(10,1);
        }
    }

    void
    run()
    {
        test();
    }
};

TEST_SUITE(static_resource_test, "boost.json.static_resource");

BOOST_JSON_NS_END
