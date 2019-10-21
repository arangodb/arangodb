/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_vector.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com . 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <cstddef> // NULL
#include <fstream>

#include <cstdio> // remove
#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{ 
    using ::remove;
}
#endif
#include <boost/static_assert.hpp>
#include "test_tools.hpp"

#include <boost/serialization/vector.hpp>

// normal class with default constructor
#include "A.hpp"
#include "A.ipp"

template <class T>
int test_vector_detail(const std::vector<T> & avector)
{
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    // test array of objects
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << boost::serialization::make_nvp("avector", avector);
    }
    std::vector< T > avector1;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> boost::serialization::make_nvp("avector", avector1);
    }
    BOOST_CHECK(avector == avector1);
    std::remove(testfile);
    return EXIT_SUCCESS;
}

template <class T>
int test_default_constructible()
{
    // test array of objects
    std::vector<T> avector;
    avector.push_back(T());
    avector.push_back(T());
    return test_vector_detail(avector);
}

// class without default constructor
struct X {
    //BOOST_DELETED_FUNCTION(X());
public:
    int m_i;
    X(const X & x) :
        m_i(x.m_i)
    {}
    X(const int & i) :
        m_i(i)
    {}
    bool operator==(const X & rhs) const {
        return m_i == rhs.m_i;
    }
    template<class Archive>
    void serialize(Archive & ar, const unsigned int /*version*/){
        ar & BOOST_SERIALIZATION_NVP(m_i);
    }
};

template<class Archive>
inline void save_construct_data(
    Archive & ar, 
    const X * x,
    const unsigned int /* file_version */
){
    // variable used for construction
    ar << boost::serialization::make_nvp("i", x->m_i);
}

template<class Archive>
inline void load_construct_data(
    Archive & ar, 
    X * x,
    const unsigned int /* file_version */
){
    int i;
    ar >> boost::serialization::make_nvp("i", i);
    ::new(x)X(i);
}

int test_non_default_constructible()
{
    // test array of objects
    std::vector<X> avector;
    avector.push_back(X(123));
    avector.push_back(X(456));
    return test_vector_detail(avector);
}

int test_main( int /* argc */, char* /* argv */[] )
{
    int res;
    res = test_default_constructible<A>();
    // test an int vector for which optimized versions should be available
    if (res == EXIT_SUCCESS)
        res = test_default_constructible<int>();
    // test a bool vector
    if (res == EXIT_SUCCESS)
        res = test_default_constructible<bool>();
    if (res == EXIT_SUCCESS)
        res = test_non_default_constructible();
    return res;
}

// EOF
