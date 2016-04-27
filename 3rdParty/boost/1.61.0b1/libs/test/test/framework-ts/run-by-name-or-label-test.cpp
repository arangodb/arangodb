//  (C) Copyright Gennadiy Rozental 2003-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision: 62023 $
//
//  Description : unit test for class properties facility
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE Boost.Test run by name/label implementation test
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include <boost/test/tree/test_case_counter.hpp>
#include <boost/test/tree/traverse.hpp>

namespace utf = boost::unit_test;

void A() {}
void B() {}
void C() {}
void D() {}
void E() {}
void F() {}
void longnameA() {}
void Blongname() {}

//____________________________________________________________________________//

void
test_count( utf::test_suite* master_ts, char const** argv, int argc, unsigned expected )
{
    argc /= sizeof(char*);

    BOOST_TEST_INFO( argv[1] );
    if( argc > 2 )
        BOOST_TEST_INFO( argv[2] );

    utf::runtime_config::init( argc, (char**)argv );
    utf::framework::impl::setup_for_execution( *master_ts );

    utf::test_case_counter tcc;
    utf::traverse_test_tree( master_ts->p_id, tcc );
    BOOST_TEST( tcc.p_count == expected );
}

BOOST_AUTO_TEST_CASE( test_run_by_name )
{
    utf::test_suite* master_ts = BOOST_TEST_SUITE("local master");

    utf::test_suite* ts1 = BOOST_TEST_SUITE("ts1");
    ts1->add( BOOST_TEST_CASE( &C ) );
    ts1->add( BOOST_TEST_CASE( &D ) );

    utf::test_suite* ts2 = BOOST_TEST_SUITE("ts2");
    ts2->add( BOOST_TEST_CASE( &A ) );
    ts2->add( BOOST_TEST_CASE( &C ) );
    ts2->add( BOOST_TEST_CASE( &longnameA ) );
    ts2->add( ts1 );

    master_ts->add( BOOST_TEST_CASE( &A ) );
    master_ts->add( BOOST_TEST_CASE( &B ) );
    master_ts->add( BOOST_TEST_CASE( &longnameA ) );
    master_ts->add( BOOST_TEST_CASE( &Blongname ) );
    master_ts->add( ts2 );

    master_ts->p_default_status.value = utf::test_unit::RS_ENABLED;
    utf::framework::finalize_setup_phase( master_ts->p_id );
    {
        utf::framework::impl::setup_for_execution( *master_ts );
        utf::test_case_counter tcc;
        utf::traverse_test_tree( master_ts->p_id, tcc );
        BOOST_TEST( tcc.p_count == 9U );
    }

    {
        char const* argv[] = { "a.exe", "--run=*" };
        test_count( master_ts, argv, sizeof(argv), 9 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!*" };
        test_count( master_ts, argv, sizeof(argv), 0 );
    }

    {
        char const* argv[] = { "a.exe", "--run=*/*" };
        test_count( master_ts, argv, sizeof(argv), 5 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!*/*" };
        test_count( master_ts, argv, sizeof(argv), 4 );
    }

    {
        char const* argv[] = { "a.exe", "--run=*/*/*" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }
    {
        char const* argv[] = { "a.exe", "--run=!*/*/*" };
        test_count( master_ts, argv, sizeof(argv), 7 );
    }

    {
        char const* argv[] = { "a.exe", "--run=klmn" };
        test_count( master_ts, argv, sizeof(argv), 0 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!klmn" };
        test_count( master_ts, argv, sizeof(argv), 9 );
    }

    {
        char const* argv[] = { "a.exe", "--run=A" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!A" };
        test_count( master_ts, argv, sizeof(argv), 8 );
    }

    {
        char const* argv[] = { "a.exe", "--run=*A" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!*A" };
        test_count( master_ts, argv, sizeof(argv), 7 );
    }

    {
        char const* argv[] = { "a.exe", "--run=B*" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!B*" };
        test_count( master_ts, argv, sizeof(argv), 7 );
    }

    {
        char const* argv[] = { "a.exe", "--run=*ngn*" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2" };
        test_count( master_ts, argv, sizeof(argv), 5 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2" };
        test_count( master_ts, argv, sizeof(argv), 4 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/*" };
        test_count( master_ts, argv, sizeof(argv), 5 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/*" };
        test_count( master_ts, argv, sizeof(argv), 4 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/C" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/C" };
        test_count( master_ts, argv, sizeof(argv), 8 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/*A" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/*A" };
        test_count( master_ts, argv, sizeof(argv), 7 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/ts1" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/ts1" };
        test_count( master_ts, argv, sizeof(argv), 7 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/ts1/C" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/ts1/C" };
        test_count( master_ts, argv, sizeof(argv), 8 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/ts1/*D*" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/ts1/*D*" };
        test_count( master_ts, argv, sizeof(argv), 8 );
    }

    {
        char const* argv[] = { "a.exe", "--run=A,B" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!A,B" };
        test_count( master_ts, argv, sizeof(argv), 7 );
    }

    {
        char const* argv[] = { "a.exe", "--run=*A,B" };
        test_count( master_ts, argv, sizeof(argv), 3 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!*A,B" };
        test_count( master_ts, argv, sizeof(argv), 6 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/C,ts1" };
        test_count( master_ts, argv, sizeof(argv), 3 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/C,ts1" };
        test_count( master_ts, argv, sizeof(argv), 6 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/C,ts1/D" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts2/C,ts1/D" };
        test_count( master_ts, argv, sizeof(argv), 8 );
    }

    {
        char const* argv[] = { "a.exe", "--run=A", "--run=B" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!A", "--run=!B" };
        test_count( master_ts, argv, sizeof(argv), 7 );
    }

    {
        char const* argv[] = { "a.exe", "--run=A", "--run=ts2/ts1/C" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2", "--run=!ts2/*A" };
        test_count( master_ts, argv, sizeof(argv), 3 );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_run_by_label )
{
    utf::test_case* tc;

    utf::test_suite* master_ts = BOOST_TEST_SUITE("local master");

    utf::test_suite* ts1 = BOOST_TEST_SUITE("ts1");
    ts1->add_label( "f2" );
    ts1->add( tc=BOOST_TEST_CASE( &C ) );
    tc->add_label( "L1" );
    ts1->add( tc=BOOST_TEST_CASE( &D ) );
    tc->add_label( "L1" );
    tc->add_label( "l2" );

    utf::test_suite* ts2 = BOOST_TEST_SUITE("ts2");
    ts2->add_label( "FAST" );
    ts2->add( tc=BOOST_TEST_CASE( &A ) );
    tc->add_label( "L1" );
    ts2->add( BOOST_TEST_CASE( &C ) );
    ts2->add( BOOST_TEST_CASE( &longnameA ) );
    ts2->add( ts1 );

    master_ts->add( BOOST_TEST_CASE( &A ) );
    master_ts->add( BOOST_TEST_CASE( &B ) );
    master_ts->add( tc=BOOST_TEST_CASE( &longnameA ) );
    tc->add_label( "f2" );
    master_ts->add( BOOST_TEST_CASE( &Blongname ) );
    master_ts->add( ts2 );


    master_ts->p_default_status.value = utf::test_unit::RS_ENABLED;
    utf::framework::finalize_setup_phase( master_ts->p_id );

    {
        char const* argv[] = { "a.exe", "--run=@L1" };
        test_count( master_ts, argv, sizeof(argv), 3 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!@INV" };
        test_count( master_ts, argv, sizeof(argv), 9 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!@L1" };
        test_count( master_ts, argv, sizeof(argv), 6 );
    }

    {
        char const* argv[] = { "a.exe", "--run=@l2" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=@inval" };
        test_count( master_ts, argv, sizeof(argv), 0 );
    }

    {
        char const* argv[] = { "a.exe", "--run=@FAST" };
        test_count( master_ts, argv, sizeof(argv), 5 );
    }

    {
        char const* argv[] = { "a.exe", "--run=@f2" };
        test_count( master_ts, argv, sizeof(argv), 3 );
    }

    {
        char const* argv[] = { "a.exe", "--run=@L1", "--run=@l2" };
        test_count( master_ts, argv, sizeof(argv), 3 );
    }

    {
        char const* argv[] = { "a.exe", "--run=@L1", "--run=!@l2" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_dependency_handling )
{
    utf::test_case* tc;
    utf::test_case* tcB;

    //            M             |
    //         /  |  \          |
    //        /   |   \         |
    //     TS2   TS4  TS3       |
    //     / \    |   / \       |
    //    C   D  TS1 E   F      |
    //           / \            |
    //          A   B           |
    //
    //  D => TS1
    //  B => F

    utf::test_suite* master_ts = BOOST_TEST_SUITE("local master");

    utf::test_suite* ts1 = BOOST_TEST_SUITE("ts1");
    ts1->add( BOOST_TEST_CASE( &A ) );
    ts1->add( tcB=BOOST_TEST_CASE( &B ) );

    utf::test_suite* ts2 = BOOST_TEST_SUITE("ts2");
    ts2->add( BOOST_TEST_CASE( &C ) );
    ts2->add( tc=BOOST_TEST_CASE( &D ) );
    tc->depends_on( ts1 );

    utf::test_suite* ts3 = BOOST_TEST_SUITE("ts3");
    ts3->add( BOOST_TEST_CASE( &E ) );
    ts3->add( tc=BOOST_TEST_CASE( &F ) );
    tcB->depends_on( tc );

    utf::test_suite* ts4 = BOOST_TEST_SUITE("ts4");
    ts4->add( ts1 );

    master_ts->add( ts2 );
    master_ts->add( ts3 );
    master_ts->add( ts4 );

    master_ts->p_default_status.value = utf::test_unit::RS_ENABLED;
    utf::framework::finalize_setup_phase( master_ts->p_id );

    {
        char const* argv[] = { "a.exe", "--run=ts2/C" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts3" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2/C" };
        test_count( master_ts, argv, sizeof(argv), 1 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts4/ts1/B" };
        test_count( master_ts, argv, sizeof(argv), 2 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts4/ts1" };
        test_count( master_ts, argv, sizeof(argv), 3 );
    }

    {
        char const* argv[] = { "a.exe", "--run=ts2" };
        test_count( master_ts, argv, sizeof(argv), 5 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!ts3/F" };
        test_count( master_ts, argv, sizeof(argv), 5 );
    }

    {
        char const* argv[] = { "a.exe", "--run=!*/ts1" };
        test_count( master_ts, argv, sizeof(argv), 4 );
    }
}

//____________________________________________________________________________//

// EOF

