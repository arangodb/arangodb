/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_map.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// (C) Copyright 2014 Jim Bell
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <algorithm> // std::copy
#include <vector>
#include <fstream>
#include <cstddef> // size_t, NULL

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>

#include <cstdio>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::rand;
    using ::size_t;
}
#endif

#include "test_tools.hpp"

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/map.hpp>

#include "A.hpp"
#include "A.ipp"

///////////////////////////////////////////////////////
// a key value initialized with a random value for use
// in testing STL map serialization
struct random_key {
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(
        Archive & ar,
        const unsigned int /* file_version */
    ){
        ar & boost::serialization::make_nvp("random_key", m_i);
    }
    int m_i;
    random_key() : m_i(std::rand()){};
    bool operator<(const random_key &rhs) const {
        return m_i < rhs.m_i;
    }
    bool operator==(const random_key &rhs) const {
        return m_i == rhs.m_i;
    }
    operator std::size_t () const {    // required by hash_map
        return m_i;
    }
};

void
test_map(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    BOOST_MESSAGE("map");
    // test map of objects
    std::map<random_key, A> amap;
    amap.insert(std::make_pair(random_key(), A()));
    amap.insert(std::make_pair(random_key(), A()));
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << boost::serialization::make_nvp("amap", amap);
    }
    std::map<random_key, A> amap1;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> boost::serialization::make_nvp("amap", amap1);
    }
    BOOST_CHECK(amap == amap1);
    std::remove(testfile);
}

void
test_map_2(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    BOOST_MESSAGE("map_2");
    std::pair<int, int> a(11, 22);
    std::map<int, int> b;
    b[0] = 0;
    b[-1] = -1;
    b[1] = 1;
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        std::pair<int, int> * const pa = &a;
        std::map<int, int> * const pb = &b;
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << BOOST_SERIALIZATION_NVP(pb);
        oa << BOOST_SERIALIZATION_NVP(pa);
    }
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        std::pair<int, int> *pa = 0;
        std::map<int, int> *pb = 0;
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> BOOST_SERIALIZATION_NVP(pb);
        ia >> BOOST_SERIALIZATION_NVP(pa);
        delete pa;
        delete pb;
    }
    std::remove(testfile);
}

void
test_multimap(){
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    BOOST_MESSAGE("multimap");
    std::multimap<random_key, A> amultimap;
    amultimap.insert(std::make_pair(random_key(), A()));
    amultimap.insert(std::make_pair(random_key(), A()));
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << boost::serialization::make_nvp("amultimap", amultimap);
    }
    std::multimap<random_key, A> amultimap1;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> boost::serialization::make_nvp("amultimap", amultimap1);
    }
    BOOST_CHECK(amultimap == amultimap1);
    std::remove(testfile);
}

int test_main( int /* argc */, char* /* argv */[] )
{
    test_map();
    test_map_2();
    test_multimap();

    return EXIT_SUCCESS;
}
