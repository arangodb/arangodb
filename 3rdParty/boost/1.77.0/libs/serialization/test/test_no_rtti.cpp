/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_no_rtti.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// note: this program tests the inter-operability of different
// extended typeinfo systems.  In this example, one class is
// identified using the default RTTI while the other uses a custom
// system based on the export key.
//
// As this program uses RTTI for one of the types, the test will fail
// on a system for which RTTI is not enabled or not existent.

#include <cstddef>
#include <fstream>
#include <iostream>

#include <boost/config.hpp>
#include <cstdio> // remove
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::remove;
}
#endif

#include <boost/serialization/type_info_implementation.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/nvp.hpp>

#include "test_tools.hpp"

#include <boost/archive/polymorphic_oarchive.hpp>
#include <boost/archive/polymorphic_iarchive.hpp>

#include "polymorphic_base.hpp"

#include "polymorphic_derived1.hpp"

#include "polymorphic_derived2.hpp"

// save derived polymorphic class
void save_derived(const char *testfile)
{
    test_ostream os(testfile, TEST_STREAM_FLAGS);
    test_oarchive oa_implementation(os, TEST_ARCHIVE_FLAGS);
    boost::archive::polymorphic_oarchive & oa_interface = oa_implementation;

    polymorphic_derived1 *rd1 = new polymorphic_derived1;
    polymorphic_derived2 *rd2 = new polymorphic_derived2;

    std::cout << "saving polymorphic_derived1 (no_rtti)\n";
    oa_interface << BOOST_SERIALIZATION_NVP(rd1);

    std::cout << "saving polymorphic_derived2\n";
    oa_interface << BOOST_SERIALIZATION_NVP(rd2);

    const polymorphic_base *rb1 =  rd1;
    polymorphic_base *rb2 = rd2;
    std::cout << "saving polymorphic_derived1 (no_rtti) through base (rtti)\n";
    oa_interface << BOOST_SERIALIZATION_NVP(rb1);
    std::cout << "saving polymorphic_derived2 through base\n";
    oa_interface << BOOST_SERIALIZATION_NVP(rb2);

    delete rd1;
    delete rd2;
}

void load_derived(const char *testfile)
{
    test_istream is(testfile, TEST_STREAM_FLAGS);
    test_iarchive  ia_implementation(is, TEST_ARCHIVE_FLAGS);
    boost::archive::polymorphic_iarchive & ia_interface = ia_implementation;

    polymorphic_derived1 *rd1 = NULL;
    polymorphic_derived2 *rd2 = NULL;

    std::cout << "loading polymorphic_derived1 (no_rtti)\n";
    ia_interface >> BOOST_SERIALIZATION_NVP(rd1);
    BOOST_CHECK_MESSAGE(
        boost::serialization::type_info_implementation<
            polymorphic_derived1
        >::type::get_const_instance()
        ==
        * boost::serialization::type_info_implementation<
            polymorphic_derived1
        >::type::get_const_instance().get_derived_extended_type_info(*rd1)
        ,
        "restored pointer d1 not of correct type"
    );

    std::cout << "loading polymorphic_derived2\n";
    ia_interface >> BOOST_SERIALIZATION_NVP(rd2);
    BOOST_CHECK_MESSAGE(
        boost::serialization::type_info_implementation<
            polymorphic_derived2
        >::type::get_const_instance()
        ==
        * boost::serialization::type_info_implementation<
            polymorphic_derived2
        >::type::get_const_instance().get_derived_extended_type_info(*rd2)
        ,
        "restored pointer d2 not of correct type"
    );
    polymorphic_base *rb1 = NULL;
    polymorphic_base *rb2 = NULL;

    // the above opereration registers the derived classes as a side
    // effect.  Hence, instances can now be correctly serialized through
    // a base class pointer.
    std::cout << "loading polymorphic_derived1 (no_rtti) through base (no_rtti)\n";
    ia_interface >> BOOST_SERIALIZATION_NVP(rb1);

    BOOST_CHECK_MESSAGE(
        rb1 == dynamic_cast<polymorphic_base *>(rd1),
        "serialized pointers not correctly restored"
    );

    BOOST_CHECK_MESSAGE(
        boost::serialization::type_info_implementation<
            polymorphic_derived1
        >::type::get_const_instance()
        ==
        * boost::serialization::type_info_implementation<
            polymorphic_base
        >::type::get_const_instance().get_derived_extended_type_info(*rb1)
        ,
        "restored pointer b1 not of correct type"
    );
    std::cout << "loading polymorphic_derived2 through base (no_rtti)\n";
    ia_interface >> BOOST_SERIALIZATION_NVP(rb2);

    BOOST_CHECK_MESSAGE(
        rb2 ==  dynamic_cast<polymorphic_base *>(rd2),
        "serialized pointers not correctly restored"
    );
    BOOST_CHECK_MESSAGE(
        boost::serialization::type_info_implementation<
            polymorphic_derived2
        >::type::get_const_instance()
        ==
        * boost::serialization::type_info_implementation<
            polymorphic_base
        >::type::get_const_instance().get_derived_extended_type_info(*rb2)
        ,
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

    save_derived(testfile);
    load_derived(testfile);
    std::remove(testfile);
    return EXIT_SUCCESS;
}

// EOF
