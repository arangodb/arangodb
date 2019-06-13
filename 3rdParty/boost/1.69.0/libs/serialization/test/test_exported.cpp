/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_exported.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com . 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <cstddef>
#include <fstream>

#include <boost/config.hpp>
#include <cstdio> // remove
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{ 
    using ::remove;
}
#endif

#include <boost/serialization/type_info_implementation.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/base_object.hpp>

#include "test_tools.hpp"

#include <boost/archive/polymorphic_oarchive.hpp>
#include <boost/archive/polymorphic_iarchive.hpp>

#define POLYMORPHIC_BASE_IMPORT
#include "polymorphic_base.hpp"

#define POLYMORPHIC_DERIVED1_IMPORT
#include "polymorphic_derived1.hpp"

#define POLYMORPHIC_DERIVED2_IMPORT
#include "polymorphic_derived2.hpp"

// save exported polymorphic class
void save_exported(const char *testfile)
{
    test_ostream os(testfile, TEST_STREAM_FLAGS);
    test_oarchive oa_implementation(os, TEST_ARCHIVE_FLAGS);
    boost::archive::polymorphic_oarchive & oa_interface = oa_implementation;

    const polymorphic_base *rb1 = new polymorphic_derived1;
    const polymorphic_base *rb2 = new polymorphic_derived2;

    // export will permit correct serialization
    // through a pointer to a base class
    std::cout << "saving polymorphic_derived1 (no_rtti)\n";
    oa_interface << BOOST_SERIALIZATION_NVP(rb1);

    std::cout << "saving polymorphic_derived2\n";
    oa_interface << BOOST_SERIALIZATION_NVP(rb2);

    delete rb1;
    delete rb2;
}

// save exported polymorphic class
void load_exported(const char *testfile)
{
    test_istream is(testfile, TEST_STREAM_FLAGS);
    test_iarchive  ia_implementation(is, TEST_ARCHIVE_FLAGS);
    boost::archive::polymorphic_iarchive & ia_interface = ia_implementation;

    polymorphic_base *rb1 = NULL;
    polymorphic_base *rb2 = NULL;

    // export will permit correct serialization
    // through a pointer to a base class
    ia_interface >> BOOST_SERIALIZATION_NVP(rb1);
    BOOST_CHECK_MESSAGE(
        boost::serialization::type_info_implementation<polymorphic_derived1>
            ::type::get_const_instance()
        == 
        * boost::serialization::type_info_implementation<polymorphic_base>
            ::type::get_const_instance().get_derived_extended_type_info(*rb1),
        "restored pointer b1 not of correct type"
    );

    ia_interface >> BOOST_SERIALIZATION_NVP(rb2);
    BOOST_CHECK_MESSAGE(
        boost::serialization::type_info_implementation<polymorphic_derived2>
            ::type::get_const_instance()
        == 
        * boost::serialization::type_info_implementation<polymorphic_base>
            ::type::get_const_instance().get_derived_extended_type_info(*rb2),
        "restored pointer b2 not of correct type"
    );

    delete rb1;
    delete rb2;
}

int
test_main( int /* argc */, char* /* argv */[] )
{
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    save_exported(testfile);
    load_exported(testfile);
    std::remove(testfile);
    return EXIT_SUCCESS;
}

// EOF
