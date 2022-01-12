/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_array.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <stdlib.h>

#include <boost/config.hpp>
#include <cstddef>
#include <fstream>
#include <algorithm> // equal
#include <cstdio> // remove
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::remove;
}
#endif
#include "test_tools.hpp"
#include <boost/core/no_exceptions_support.hpp>
#include <boost/archive/archive_exception.hpp>
#include <boost/serialization/boost_array.hpp>

#include "A.hpp"
#include "A.ipp"

template <class T>
int test_boost_array(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    // test array of objects
    const boost::array<T,10> a_array = {{T(),T(),T(),T(),T(),T(),T(),T(),T(),T()}};
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << boost::serialization::make_nvp("a_array", a_array);
    }
    {
        boost::array<T,10> a_array1;
        test_istream is(testfile, TEST_STREAM_FLAGS);
        {
            test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
            ia >> boost::serialization::make_nvp("a_array", a_array1);
        }
        is.close();
        BOOST_CHECK(std::equal(a_array.begin(), a_array.end(), a_array1.begin()));
    }
    {
        boost::array<T, 9> a_array1;
        test_istream is(testfile, TEST_STREAM_FLAGS);
        {
            test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
            bool exception_invoked = false;
            BOOST_TRY {
                ia >> boost::serialization::make_nvp("a_array", a_array1);
            }
            BOOST_CATCH (boost::archive::archive_exception const& ae){
                BOOST_CHECK(
                    boost::archive::archive_exception::array_size_too_short
                    == ae.code
                );
                exception_invoked = true;
            }
            BOOST_CATCH_END
            BOOST_CHECK(exception_invoked);
        }
        is.close();
    }
    std::remove(testfile);
    return EXIT_SUCCESS;
}

int test_main( int /* argc */, char* /* argv */[] ){
    int res;

    // boost array
    res = test_boost_array<A>();
    if (res != EXIT_SUCCESS)
        return EXIT_FAILURE;
    // test an int array for which optimized versions should be available
    res = test_boost_array<int>();
    if (res != EXIT_SUCCESS)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

// EOF
