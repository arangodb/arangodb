// (C) Copyright Raffi Enficiaud 2019.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Extends #12092 with arbitrary type list
// see https://svn.boost.org/trac10/ticket/13418 and
// https://github.com/boostorg/test/issues/141
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE template_test_case_with_variadic
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/results_collector.hpp>
#include <boost/test/utils/nullstream.hpp>
typedef boost::onullstream onullstream_type;

#include <boost/mpl/integral_c.hpp>

// tuple already done in another test module
// #include <tuple>



namespace ut = boost::unit_test;
namespace mpl = boost::mpl;

#include <iostream>

struct logger_guard {
    logger_guard(std::ostream& s_out) {
        ut::unit_test_log.set_stream( s_out );
    }
    ~logger_guard() {
        ut::unit_test_log.set_stream( std::cout );
    }
};

template <class ... T>
struct dummy1 {

};


//____________________________________________________________________________//

BOOST_TEST_CASE_TEMPLATE_FUNCTION( test0, Number )
{
    BOOST_TEST( 2 == (int)Number::value );
}

BOOST_AUTO_TEST_CASE( test0_only_2 )
{
    onullstream_type    null_output;
    logger_guard G(null_output);

    typedef dummy1< mpl::integral_c<int,2> > only_2;

    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    test->add( BOOST_TEST_CASE_TEMPLATE( test0, only_2 ) );

    test->p_default_status.value = ut::test_unit::RS_ENABLED;
    ut::framework::finalize_setup_phase( test->p_id );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 0U );
    BOOST_TEST( !tr.p_aborted );
}

BOOST_AUTO_TEST_CASE( test1_with_9_errors )
{
    onullstream_type    null_output;
    logger_guard G(null_output);

    typedef dummy1<
      mpl::integral_c<int,0>,
      mpl::integral_c<int,1>,
      mpl::integral_c<int,2>,
      mpl::integral_c<int,3>,
      mpl::integral_c<int,4>,
      mpl::integral_c<int,5>,
      mpl::integral_c<int,6>,
      mpl::integral_c<int,7>,
      mpl::integral_c<int,8>,
      mpl::integral_c<int,9>
    > range_10;

    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    test->add( BOOST_TEST_CASE_TEMPLATE( test0, range_10 ) );

    test->p_default_status.value = ut::test_unit::RS_ENABLED;
    ut::framework::finalize_setup_phase( test->p_id );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 9U );
    BOOST_TEST( !tr.p_aborted );
}


int counter = 0;
BOOST_TEST_CASE_TEMPLATE_FUNCTION( test_counter, Number )
{
    BOOST_TEST( counter++ == (int)Number::value );
}

BOOST_AUTO_TEST_CASE( test_left_to_right_evaluation )
{
    onullstream_type null_output;
    logger_guard G(null_output);

    typedef dummy1<
      mpl::integral_c<int,0>,
      mpl::integral_c<int,1>,
      mpl::integral_c<int,2>,
      mpl::integral_c<int,3>,
      mpl::integral_c<int,4>,
      mpl::integral_c<int,5>,
      mpl::integral_c<int,6>,
      mpl::integral_c<int,7>,
      mpl::integral_c<int,8>,
      mpl::integral_c<int,9>
    > range_10;

    ut::test_suite* test = BOOST_TEST_SUITE( "" );

    test->add( BOOST_TEST_CASE_TEMPLATE( test_counter, range_10 ) );

    test->p_default_status.value = ut::test_unit::RS_ENABLED;
    ut::framework::finalize_setup_phase( test->p_id );
    ut::framework::run( test );
    ut::test_results const& tr = ut::results_collector.results( test->p_id );

    ut::unit_test_log.set_stream( std::cout );
    BOOST_TEST( tr.p_assertions_failed == 0U );
    BOOST_TEST( !tr.p_aborted );
}


typedef dummy1<
  mpl::integral_c<int,1>,
  mpl::integral_c<int,3>,
  mpl::integral_c<int,5>,
  mpl::integral_c<int,6>,
  mpl::integral_c<int,7>,
  mpl::integral_c<int,8>,
  mpl::integral_c<int,9>
> range_special;

BOOST_AUTO_TEST_CASE_TEMPLATE(odd_or_above_5, T, range_special) {
    BOOST_TEST( (T::value % 2 || T::value >= 5 ) );
}
