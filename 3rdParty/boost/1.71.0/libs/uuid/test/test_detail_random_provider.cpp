//
// Copyright (c) 2017 James E. King III
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//   https://www.boost.org/LICENSE_1_0.txt)
//
// Positive and negative testing for detail::random_provider
//

#include <boost/array.hpp>
#include <boost/cstdint.hpp>
#include <boost/detail/lightweight_test.hpp>

// The mock needs to load first for posix system call redirection to work properly
#include "mock_random.hpp"
#include <boost/uuid/detail/random_provider.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <limits>
#include <string.h>


int main(int, char*[])
{
#if !defined(BOOST_UUID_TEST_RANDOM_MOCK)   // Positive Testing

    boost::uuids::detail::random_provider provider;
    boost::array<unsigned int, 2> ints;

    // test generator()
    ints[0] = 0;
    ints[1] = 0;
    provider.generate(ints.begin(), ints.end());
    BOOST_TEST_NE(ints[0], ints[1]);

    // test name()
    BOOST_TEST_GT(strlen(provider.name()), 4u);

    // test get_random_bytes()
    char buf1[64];
    char buf2[64];
    provider.get_random_bytes(buf1, 64);
    provider.get_random_bytes(buf2, 64);
    BOOST_TEST_NE(0, memcmp(buf1, buf2, 64));

#else                                       // Negative Testing

    if (expectations_capable())
    {
        // Test fail acquiring context if the provider supports it
        if (provider_acquires_context())
        {
            expect_next_call_success(false);
            BOOST_TEST_THROWS(boost::uuids::detail::random_provider(), 
                              boost::uuids::entropy_error);
            BOOST_TEST(expectations_met());
        }

        // Test fail acquiring entropy
        if (provider_acquires_context())
        {
            expect_next_call_success(true);
        }
        expect_next_call_success(false);
        // 4 is important for the posix negative test (partial read) to work properly
        // as it sees a size of 4, returns 1, causing a 2nd loop to read 3 more bytes...
        char buf[4];
        BOOST_TEST_THROWS(boost::uuids::detail::random_provider().get_random_bytes(buf, 4), 
                          boost::uuids::entropy_error);
        BOOST_TEST(expectations_met());
    }

#endif

    return boost::report_errors();
}
