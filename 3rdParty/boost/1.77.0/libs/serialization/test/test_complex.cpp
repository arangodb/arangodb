/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_complex.cpp

// (C) Copyright 2005 Matthias Troyer .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <fstream>

#include <cstddef> // NULL
#include <cstdlib> // rand
#include <cstdio> // remove
#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/math/special_functions/next.hpp>

#if defined(BOOST_NO_STDC_NAMESPACE)
#include <boost/limits.hpp>
namespace std{
    using ::rand;
    using ::remove;
    #if BOOST_WORKAROUND(BOOST_MSVC, >= 1400) && !defined(UNDER_CE)
        using ::numeric_limits;
    #endif
}
#endif

#include "test_tools.hpp"

#include <boost/preprocessor/stringize.hpp>
#include BOOST_PP_STRINGIZE(BOOST_ARCHIVE_TEST)

#include <boost/serialization/complex.hpp>

#include <iostream>

int test_main( int /* argc */, char* /* argv */[] )
{
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    // test array of objects
    std::complex<float> a(
        static_cast<float>(std::rand()) / static_cast<float>(std::rand()),
        static_cast<float>(std::rand()) / static_cast<float>(std::rand())
    );
    std::complex<double> b(
        static_cast<double>(std::rand()) / static_cast<double>(std::rand()),
        static_cast<double>(std::rand()) / static_cast<double>(std::rand())
    );
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os);
        oa << boost::serialization::make_nvp("afloatcomplex", a);
        oa << boost::serialization::make_nvp("adoublecomplex", b);
    }
    std::complex<float> a1;
    std::complex<double> b1;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is);
        ia >> boost::serialization::make_nvp("afloatcomplex", a1);
        ia >> boost::serialization::make_nvp("adoublecomplex", b1);
    }

    std::cerr << "a.real()-a1a.real() distance = " << std::abs( boost::math::float_distance(a.real(), a1.real())) << std::endl;
    BOOST_CHECK(std::abs(boost::math::float_distance(a.real(), a1.real())) < 2);
    std::cerr << "a.imag() - a1a.imag() distance = " << std::abs( boost::math::float_distance(a.imag(), a1.imag())) << std::endl;
    BOOST_CHECK(std::abs(boost::math::float_distance(a.imag(), a1.imag())) < 2);
    std::cerr << "b.real() - b1.real() distance = " << std::abs( boost::math::float_distance(b.real(), b1.real())) << std::endl;
    BOOST_CHECK(std::abs(boost::math::float_distance(b.real(), b1.real())) < 2);
    std::cerr << "b.imag() - b1.imag() distance = " << std::abs( boost::math::float_distance(b.imag(), b1.imag())) << std::endl;
    BOOST_CHECK(std::abs(boost::math::float_distance(b.imag(), b1.imag())) < 2);

    std::remove(testfile);
    return EXIT_SUCCESS;
}

// EOF
