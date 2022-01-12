//
// Copyright (c) 2020 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/null_resource.hpp>

#include <boost/json/storage_ptr.hpp>

#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

class null_resource_test
{
public:
    void
    test()
    {
        auto& mr = *get_null_resource();
        BOOST_TEST_THROWS(
            mr.allocate(16),
            std::bad_alloc);
        char buf[128];
        // no-op
        mr.deallocate(&buf[0], 128);
        BOOST_TEST(
            mr == *get_null_resource());
    }

    void
    run()
    {
        test();
    }
};

TEST_SUITE(null_resource_test, "boost.json.null_resource");

BOOST_JSON_NS_END
