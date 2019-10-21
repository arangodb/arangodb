//  (C) Copyright Raffi Enficiaud 2018.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! checking the clashing names, ticket trac #12597
// *****************************************************************************

#define BOOST_TEST_MODULE test_clashing_names
#include <boost/test/unit_test.hpp>
#include <boost/test/utils/string_cast.hpp>
#include <boost/mpl/list.hpp>
#include <boost/bind.hpp>

void suite1_test1()
{
  BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE( test_clashing_suites )
{
    using namespace boost::unit_test;
    test_suite* master_ts = BOOST_TEST_SUITE("local master");
    test_suite* t_suite1 = BOOST_TEST_SUITE( "suite1" );
    test_suite* t_suite2 = BOOST_TEST_SUITE( "suite1" );
    master_ts->add(t_suite1);
    BOOST_CHECK_NO_THROW( master_ts->add(t_suite2) );

    master_ts->p_default_status.value = test_unit::RS_ENABLED;
    BOOST_CHECK_THROW(framework::finalize_setup_phase( master_ts->p_id ),
                      boost::unit_test::framework::setup_error );
}

BOOST_AUTO_TEST_CASE( test_clashing_cases )
{
    using namespace boost::unit_test;
    test_suite* master_ts = BOOST_TEST_SUITE("local master");
    test_suite* t_suite1 = BOOST_TEST_SUITE( "suite1" );
    test_suite* t_suite2 = BOOST_TEST_SUITE( "suite2" );
    master_ts->add(t_suite1);
    master_ts->add(t_suite2);

    t_suite1->add( BOOST_TEST_CASE( suite1_test1 ) );
    BOOST_CHECK_NO_THROW( t_suite1->add( BOOST_TEST_CASE( suite1_test1 ) ) );
    BOOST_CHECK_THROW(framework::finalize_setup_phase( master_ts->p_id ),
                      boost::unit_test::framework::setup_error );

    BOOST_CHECK_NO_THROW( t_suite2->add( BOOST_TEST_CASE( suite1_test1 ) ) );
}

BOOST_TEST_CASE_TEMPLATE_FUNCTION( template_test_case, T )
{
    BOOST_TEST( sizeof(T) == 4U );
}

BOOST_AUTO_TEST_CASE( test_clashing_cases_template_test_case )
{
    using namespace boost::unit_test;
    test_suite* master_ts = BOOST_TEST_SUITE("local master");
    test_suite* t_suite1 = BOOST_TEST_SUITE( "suite1" );
    test_suite* t_suite2 = BOOST_TEST_SUITE( "suite2" );
    master_ts->add(t_suite1);
    master_ts->add(t_suite2);

    typedef boost::mpl::list<int, long, unsigned char> test_types1;
    typedef boost::mpl::list<int, long, unsigned char, int> test_types2;

    BOOST_CHECK_NO_THROW( t_suite2->add( BOOST_TEST_CASE_TEMPLATE( template_test_case, test_types1 ) ) );
    BOOST_CHECK_NO_THROW(framework::finalize_setup_phase( master_ts->p_id ));

    BOOST_CHECK_NO_THROW( t_suite1->add( BOOST_TEST_CASE_TEMPLATE( template_test_case, test_types2 ) ) );
    BOOST_CHECK_THROW(framework::finalize_setup_phase( master_ts->p_id ),
                      boost::unit_test::framework::setup_error );
}

void test2(int value)
{
  BOOST_TEST(value >= 0);
}


BOOST_AUTO_TEST_CASE( test_clashing_cases_with_name )
{
    using namespace boost::unit_test;
    test_suite* master_ts = BOOST_TEST_SUITE("local master");

    test_suite* t_suite1 = BOOST_TEST_SUITE( "suite1" );
    master_ts->add(t_suite1);
    t_suite1->add( BOOST_TEST_CASE( suite1_test1 ) );
    t_suite1->add( BOOST_TEST_CASE( boost::bind( test2, 0 ) ) );
    BOOST_CHECK_NO_THROW( t_suite1->add( BOOST_TEST_CASE( boost::bind( test2, 0 ) ) ) );
    BOOST_CHECK_THROW(framework::finalize_setup_phase( master_ts->p_id ),
                      boost::unit_test::framework::setup_error );

    for(int i = 0; i < 10; i++) {
        t_suite1->add( BOOST_TEST_CASE_NAME( boost::bind( test2, i ), "test-X-" + boost::unit_test::utils::string_cast(i) ) );
    }
}
