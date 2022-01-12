/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_set.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// (C) Copyright 2014 Jim Bell
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <cstddef> // NULLsize_t
#include <cstdio> // remove
#include <fstream>

#include <algorithm> // std::copy
#include <vector>

#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::size_t;
}
#endif

#include <boost/detail/workaround.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::remove;
}
#endif

#include <boost/archive/archive_exception.hpp>

#include "test_tools.hpp"

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/set.hpp>

#include "A.hpp"
#include "A.ipp"

void
test_set(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    // test array of objects
    std::set<A> aset;
    aset.insert(A());
    aset.insert(A());
    const A * a_ptr = & * aset.begin();
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << boost::serialization::make_nvp("aset", aset);
        // serialize a pointer into the set
        oa << boost::serialization::make_nvp("a_ptr", a_ptr);
    }
    std::set<A> aset1;
    A * a_ptr1;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> boost::serialization::make_nvp("aset", aset1);
        // deserialize a pointer into the set
        ia >> boost::serialization::make_nvp("a_ptr1", a_ptr1);
    }
    BOOST_CHECK_EQUAL(aset, aset1);
    BOOST_CHECK_EQUAL(*a_ptr1, * aset1.begin());
    BOOST_CHECK_EQUAL(a_ptr1, & * aset1.begin());
    std::remove(testfile);
}

void
test_multiset(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    std::multiset<A> amultiset;
    amultiset.insert(A());
    amultiset.insert(A());
    const A * a_ptr = & * amultiset.begin();
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << boost::serialization::make_nvp("amultiset", amultiset);
        // serialize a pointer into the set
        oa << boost::serialization::make_nvp("a_ptr", a_ptr);
    }
    std::multiset<A> amultiset1;
    A * a_ptr1;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> boost::serialization::make_nvp("amultiset", amultiset1);
        // deserialize a pointer into the set
        ia >> boost::serialization::make_nvp("a_ptr1", a_ptr1);
    }
    BOOST_CHECK(amultiset == amultiset1);
    BOOST_CHECK_EQUAL(*a_ptr1, * amultiset1.begin());
    BOOST_CHECK_EQUAL(a_ptr1, & * amultiset1.begin());
    std::remove(testfile);
}

int test_main( int /* argc */, char* /* argv */[] ){
    test_set();
    test_multiset();

    return EXIT_SUCCESS;
}
