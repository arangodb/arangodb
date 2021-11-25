//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/memory_resource.hpp>
#include <boost/json/monotonic_resource.hpp>
#include <boost/json/value.hpp>

#ifndef BOOST_JSON_STANDALONE
#include <boost/container/pmr/vector.hpp>
#endif
#include <vector>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class memory_resource_test
{
public:
    void
    testBoostPmr()
    {
#ifndef BOOST_JSON_STANDALONE
        using allocator_type =
            polymorphic_allocator<value>;

        // pass polymorphic_allocator
        // where storage_ptr is expected
        {
            monotonic_resource mr;
            value  jv( allocator_type{&mr} );
            object  o( allocator_type{&mr} );
            array   a( allocator_type{&mr} );
            string  s( allocator_type{&mr} );
        }
        {
            monotonic_resource mr;
            allocator_type a(&mr);

            boost::container::pmr::vector<value> v1(a);
            v1.resize(3);
            BOOST_TEST(v1[1].storage().get() == &mr);

            std::vector<value, allocator_type> v2(3, {}, a);
            BOOST_TEST(v2[1].storage().get() == &mr);
        }
#endif
    }

    void
    testStdPmr()
    {
#ifdef BOOST_JSON_STANDALONE
        using allocator_type =
            std::pmr::polymorphic_allocator<value>;

        // pass polymorphic_allocator
        // where storage_ptr is expected
        {
            value  jv( allocator_type{} );
            object  o( allocator_type{} );
            array   a( allocator_type{} );
            string  s( allocator_type{} );
        }
        {
            monotonic_resource mr;
            allocator_type a(&mr);

            std::vector<value, allocator_type> v2(3, {}, a);
            BOOST_TEST(v2[1].storage().get() == &mr);
        }
#endif
    }

    // These are here instead of the type-specific
    // test TUs, so that we only need to link to
    // Boost.Container from one file.
    void
    testPmr()
    {
        // array
        {
            // get_allocator
            {
                monotonic_resource mr;
                array a(&mr);
                BOOST_TEST(a.get_allocator().resource() == &mr);
            }
        }

        // object
        {
            // get_allocator
            {
                monotonic_resource mr;
                object o(&mr);
                BOOST_TEST(o.get_allocator().resource() == &mr);
            }
        }

        // string
        {
            // get_allocator
            {
                monotonic_resource mr;
                string s(&mr);
                BOOST_TEST(s.get_allocator().resource() == &mr);
            }
        }

        // value
        {
            // get_allocator
            {
                monotonic_resource mr;
                value jv(&mr);
                BOOST_TEST(jv.get_allocator().resource() == &mr);
            }
        }
    }

    void
    run()
    {
        testBoostPmr();
        testStdPmr();
        testPmr();
    }
};

TEST_SUITE(memory_resource_test, "boost.json.memory_resource");

BOOST_JSON_NS_END
