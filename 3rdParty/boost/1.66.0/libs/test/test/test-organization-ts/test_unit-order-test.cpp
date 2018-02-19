//  (C) Copyright Gennadiy Rozental 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : tests proper order of test unis in case of various dependencies specifications
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE test unit order test
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/tree/visitor.hpp>

#include <boost/test/utils/nullstream.hpp>
typedef boost::onullstream onullstream_type;
namespace ut = boost::unit_test;
namespace tt = boost::test_tools;

#include <iostream>

//____________________________________________________________________________//

void some_test() {}
#define TC( name )                                                      \
boost::unit_test::make_test_case( boost::function<void ()>(some_test),  \
                                  BOOST_TEST_STRINGIZE( name ),         \
                                  __FILE__, __LINE__ )

//____________________________________________________________________________//

struct tu_order_collector : ut::test_observer {
    virtual void    test_unit_start( ut::test_unit const& tu )
    {
        // std::cout << "## TU: " << tu.full_name() << std::endl;
        m_order.push_back( tu.p_id );
    }

    std::vector<ut::test_unit_id> m_order;
};

//____________________________________________________________________________//

static tu_order_collector
run_tree( ut::test_suite* master )
{
    tu_order_collector c;
    ut::framework::register_observer( c );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    ut::framework::finalize_setup_phase( master->p_id );

    onullstream_type    null_output;
    ut::unit_test_log.set_stream( null_output );

    //hrf_content_reporter reporter( std::cout );
    //ut::traverse_test_tree( *master, reporter, true );

    ut::framework::run( master );
    ut::unit_test_log.set_stream( std::cout );

    ut::framework::deregister_observer( c );

    return c;
}

//____________________________________________________________________________//

struct _3cases {
    _3cases() {
        master = BOOST_TEST_SUITE( "master" );

        tc1 = TC( test1 );
        tc2 = TC( test2 );
        tc3 = TC( test3 );

        master->add( tc1 );
        master->add( tc2 );
        master->add( tc3 );
    }

    void test_run( std::vector<ut::test_unit_id> const& expected_order )
    {
        tu_order_collector res = run_tree( master );

#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
        BOOST_TEST( expected_order == res.m_order, tt::per_element() );
#else
        BOOST_CHECK_EQUAL_COLLECTIONS(expected_order.begin(), expected_order.end(),
                                      res.m_order.begin(), res.m_order.end());
#endif
    }

    ut::test_suite* master;

    ut::test_case* tc1;
    ut::test_case* tc2;
    ut::test_case* tc3;
};

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_simple_order, _3cases )
{
    ut::test_unit_id order[] = {master->p_id, tc1->p_id, tc2->p_id, tc3->p_id};
    std::vector<ut::test_unit_id> expected_order(order, order+4);

    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_simple_dep1, _3cases )
{
    tc1->depends_on( tc2 );

    ut::test_unit_id order[] = {master->p_id, tc2->p_id, tc3->p_id, tc1->p_id};
    std::vector<ut::test_unit_id> expected_order(order, order+4);

    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_simple_dep2, _3cases )
{
    tc1->depends_on( tc2 );
    tc2->depends_on( tc3 );

    ut::test_unit_id order[] = {master->p_id, tc3->p_id, tc2->p_id, tc1->p_id};
    std::vector<ut::test_unit_id> expected_order(order, order+4);

    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_simple_loop1, _3cases )
{
    tc1->depends_on( tc2 );
    tc2->depends_on( tc1 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_simple_loop2, _3cases )
{
    tc1->depends_on( tc2 );
    tc2->depends_on( tc3 );
    tc3->depends_on( tc1 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

struct _2suites_4_cases {
    _2suites_4_cases() {
        master = BOOST_TEST_SUITE( "master" );

        s1 = BOOST_TEST_SUITE( "s1" );
        s2 = BOOST_TEST_SUITE( "s2" );

        tc1 = TC( test1 );
        tc2 = TC( test2 );
        tc3 = TC( test3 );
        tc4 = TC( test4 );

        s1->add( tc1 );
        s1->add( tc2 );

        s2->add( tc3 );
        s2->add( tc4 );

        master->add( s1 );
        master->add( s2 );
    }

    void test_run( std::vector<ut::test_unit_id> const& expected_order )
    {
        tu_order_collector res = run_tree( master );

#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
        BOOST_TEST( expected_order == res.m_order, tt::per_element() );
#else
        BOOST_CHECK_EQUAL_COLLECTIONS(expected_order.begin(), expected_order.end(),
                                      res.m_order.begin(), res.m_order.end());
#endif

    }

    ut::test_suite* master;
    ut::test_suite* s1;
    ut::test_suite* s2;

    ut::test_case* tc1;
    ut::test_case* tc2;
    ut::test_case* tc3;
    ut::test_case* tc4;
};

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_normal_order, _2suites_4_cases )
{
    ut::test_unit_id order[] = { master->p_id,
                                 s1->p_id, tc1->p_id, tc2->p_id,
                                 s2->p_id, tc3->p_id, tc4->p_id };
    std::vector<ut::test_unit_id> expected_order(order, order+7);

    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_simple_dep, _2suites_4_cases )
{
    tc1->depends_on( tc2 );
    tc3->depends_on( tc4 );

    ut::test_unit_id order[] = { master->p_id,
                                 s1->p_id, tc2->p_id, tc1->p_id,
                                 s2->p_id, tc4->p_id, tc3->p_id };
    std::vector<ut::test_unit_id> expected_order(order, order+7);

    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_cross_dep1, _2suites_4_cases )
{
    tc1->depends_on( tc3 );

    ut::test_unit_id order[] = { master->p_id,
                                 s2->p_id, tc3->p_id, tc4->p_id,
                                 s1->p_id, tc1->p_id, tc2->p_id };
    std::vector<ut::test_unit_id> expected_order(order, order+7);

    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_cross_dep2, _2suites_4_cases )
{
    tc2->depends_on( tc4 );
    tc1->depends_on( tc2 );

    ut::test_unit_id order[] = { master->p_id,
                                 s2->p_id, tc3->p_id, tc4->p_id,
                                 s1->p_id, tc2->p_id, tc1->p_id};

    std::vector<ut::test_unit_id> expected_order(order, order+7);
    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_on_suite_dep, _2suites_4_cases )
{
    s1->depends_on( s2 );

    ut::test_unit_id order[] = { master->p_id,
                                 s2->p_id, tc3->p_id, tc4->p_id,
                                 s1->p_id, tc1->p_id, tc2->p_id };

    std::vector<ut::test_unit_id> expected_order(order, order+7);
    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_on_case_dep, _2suites_4_cases )
{
    s1->depends_on( tc3 );

    ut::test_unit_id order[] = { master->p_id,
                                 s2->p_id, tc3->p_id, tc4->p_id,
                                 s1->p_id, tc1->p_id, tc2->p_id };

    std::vector<ut::test_unit_id> expected_order(order, order+7);
    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_case_on_suite_dep, _2suites_4_cases )
{
    tc1->depends_on( s2 );
    ut::test_unit_id order[] = { master->p_id,
                                 s2->p_id, tc3->p_id, tc4->p_id,
                                 s1->p_id, tc1->p_id, tc2->p_id };

    std::vector<ut::test_unit_id> expected_order(order, order+7);
    test_run( expected_order );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_loop1, _2suites_4_cases )
{
    tc1->depends_on( tc3 );
    tc3->depends_on( tc2 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_loop2, _2suites_4_cases )
{
    tc1->depends_on( tc3 );
    tc4->depends_on( tc2 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_loop3, _2suites_4_cases )
{
    s1->depends_on( tc3 );
    s2->depends_on( tc2 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_loop4, _2suites_4_cases )
{
    s1->depends_on( tc1 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_loop5, _2suites_4_cases )
{
    tc1->depends_on( s1 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

BOOST_FIXTURE_TEST_CASE( test_suite_loop6, _2suites_4_cases )
{
    tc1->depends_on( s2 );
    tc3->depends_on( s1 );

    master->p_default_status.value = ut::test_unit::RS_ENABLED;
    BOOST_CHECK_THROW( ut::framework::finalize_setup_phase( master->p_id ),
                       ut::framework::setup_error );
}

//____________________________________________________________________________//

// EOF
