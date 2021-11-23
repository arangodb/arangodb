/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_priority_queue.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <cstddef>
#include <fstream>

#include <cstdio> // remove
#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::remove;
}
#endif

#include "test_tools.hpp"

#include <boost/serialization/vector.hpp>
#include <boost/serialization/priority_queue.hpp>

#include "A.hpp"
#include "A.ipp"

int test_main( int /* argc */, char* /* argv */[] )
{
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    // test array of objects
    std::priority_queue<A, std::vector<A> > a_priority_queue, a_priority_queue1;
    a_priority_queue.push(A());
    a_priority_queue.push(A());
    a_priority_queue.push(A());
    a_priority_queue.push(A());
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << boost::serialization::make_nvp("a_priority_queue",a_priority_queue);
    }
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> boost::serialization::make_nvp("a_priority_queue",a_priority_queue1);
    }
    BOOST_CHECK(a_priority_queue.size() == a_priority_queue1.size());

    for(int i = a_priority_queue.size(); i-- > 0;){
        const A & a1 = a_priority_queue.top();
        const A & a2 = a_priority_queue1.top();
        BOOST_CHECK(a1 == a2);
        a_priority_queue.pop();
        a_priority_queue1.pop();
    }

    std::remove(testfile);
    return EXIT_SUCCESS;
}

// EOF
