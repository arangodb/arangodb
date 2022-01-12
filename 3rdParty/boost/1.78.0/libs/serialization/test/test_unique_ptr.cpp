/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_unique_ptr.cpp

// (C) Copyright 2002-14 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <fstream>
#include <cstdio> // remove, std::auto_ptr interface wrong in dinkumware
#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::remove;
}
#endif
#include <boost/serialization/nvp.hpp>

#include "test_tools.hpp"

/////////////////////////////////////////////////////////////
// test std::unique_ptr serialization
class A
{
private:
    friend class boost::serialization::access;
    int x;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /* file_version */){
        ar & BOOST_SERIALIZATION_NVP(x);
    }
public:
    A(){}    // default constructor
    ~A(){}   // default destructor
};

#ifndef BOOST_NO_CXX11_SMART_PTR
#include <boost/serialization/unique_ptr.hpp>

int test_main(int /* argc */, char * /* argv */[]){
    const char * filename = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != filename);

    // create  a new auto pointer to ta new object of type A
    std::unique_ptr<A> spa(new A);
    {
        test_ostream os(filename, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        oa << BOOST_SERIALIZATION_NVP(spa);
    }
    {
        // reset the unique_ptr to NULL
        // thereby destroying the object of type A
        // note that the reset automagically maintains the reference count
        spa.reset();
        test_istream is(filename, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> BOOST_SERIALIZATION_NVP(spa);
        std::remove(filename);
    }
    return EXIT_SUCCESS;
}

#else

int test_main(int /* argc */, char * /* argv */[]){
    return EXIT_SUCCESS;
}

#endif // BOOST_NO_CXX11_SMART_PTR
