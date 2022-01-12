//  (C) Copyright Raffi Enficiaud 2018.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! checking the clashing names, ticket trac #12596
// *****************************************************************************

#define BOOST_TEST_MODULE test_name_sanitize
#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

void suite1_test1()
{
  BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE( test_suites_sanitize )
{
    using namespace boost::unit_test;
    test_suite* master_ts = BOOST_TEST_SUITE("local master");
    test_suite* t_suite1 = BOOST_TEST_SUITE( "!suite1" );
    master_ts->add(t_suite1);
    BOOST_TEST( t_suite1->p_name.value == "_suite1" );
    BOOST_TEST( master_ts->get("!suite1") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("_suite1") != INV_TEST_UNIT_ID );

    test_suite* t_suite2 = BOOST_TEST_SUITE( " /suite2/@label" );
    master_ts->add(t_suite2);
    BOOST_TEST( t_suite2->p_name.value == "_suite2__label" );
    BOOST_TEST( master_ts->get(" /suite2/@label") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("_suite2__label") != INV_TEST_UNIT_ID );

    // spaces trimming
    test_suite* t_suite3 = BOOST_TEST_SUITE( " /suite3/@label  " );
    master_ts->add(t_suite3);
    BOOST_TEST( t_suite3->p_name.value == "_suite3__label" );
    BOOST_TEST( master_ts->get(" /suite3/@label  ") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("/suite3/@label") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get(" _suite3__label  ") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("_suite3__label") != INV_TEST_UNIT_ID );
}

BOOST_AUTO_TEST_CASE( test_case_sanitize )
{
    using namespace boost::unit_test;
    test_suite* master_ts = BOOST_TEST_SUITE("local master");
    test_case* tc1 = make_test_case(suite1_test1, "my@whateve!r test case", __FILE__, __LINE__);
    master_ts->add( tc1 );
    BOOST_TEST( master_ts->get("my@whateve!r test case") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("my_whateve_r test case") == tc1->p_id );

    test_case* tc2 = make_test_case(suite1_test1, "  my@whateve!r test case 2  ", __FILE__, __LINE__);
    master_ts->add( tc2 );
    BOOST_TEST( master_ts->get("my@whateve!r test case 2") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("  my_whateve_r test case 2  ") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("my_whateve_r test case 2") == tc2->p_id );

    test_case* tc3 = make_test_case(suite1_test1, "  some_type < bla, blabla>  ", __FILE__, __LINE__);
    master_ts->add( tc3 );
    BOOST_TEST( master_ts->get("some_type < bla, blabla>") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("  some_type < bla, blabla>  ") == INV_TEST_UNIT_ID );
    BOOST_TEST( master_ts->get("some_type < bla_ blabla>") == tc3->p_id );
}
