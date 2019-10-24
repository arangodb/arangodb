//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : tests singleton dataset
// ***************************************************************************

// Boost.Test
#include <boost/test/unit_test.hpp>

#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
namespace data=boost::unit_test::data;

#include "datasets-test.hpp"

//____________________________________________________________________________//

int samples1[] = {1,2,3};
int index1 = 0;

BOOST_DATA_TEST_CASE( test_case_interface_01, data::make({1,2,3}) )
{
    BOOST_TEST( sample == samples1[index1++] );
}

//____________________________________________________________________________//

std::vector<std::string> samples2 = {"qwerty","asdfg"};
int index2 = 0;

BOOST_DATA_TEST_CASE( test_case_interface_02, samples2, str )
{
    BOOST_TEST( str == samples2[index2++] );
}

//____________________________________________________________________________//

int samples3[] = {7,9};
int index3 = 0;

BOOST_DATA_TEST_CASE( test_case_interface_03, data::make({1,2,3}) + samples3, val )
{
    if( index3 < 3 )
        BOOST_TEST( val == samples1[index3] );
    else
        BOOST_TEST( val == samples3[index3-3] );

    ++index3;
}

//____________________________________________________________________________//

int index4 = 0;

BOOST_DATA_TEST_CASE( test_case_interface_04, samples2 ^ data::make({7,9}), str, intval )
{
    BOOST_TEST( str == samples2[index4] );
    BOOST_TEST( intval == samples3[index4] );

    ++index4;
}

//____________________________________________________________________________//

int index5 = 0;

BOOST_DATA_TEST_CASE( test_case_interface_05, samples1 * samples2, sample0, sample1 )
{
    BOOST_TEST( sample0 == samples1[index5/2] );
    BOOST_TEST( sample1 == samples2[index5%2] );

    ++index5;
}

//____________________________________________________________________________//

int index6 = 0;

BOOST_DATA_TEST_CASE( test_case_interface_06, samples1 * samples2 * samples3, intval, str, val2 )
{
    BOOST_TEST( intval == samples1[index6/4] );
    BOOST_TEST( str == samples2[(index6/2)%2] );
    BOOST_TEST( val2 == samples3[index6%2] );

    ++index6;
}

//____________________________________________________________________________//

// test dataset dim > 3
int index7 = 0;

float samples4[] = {1E3f, 1E-3f, 3.14f};

#define sizeoftable(x) (sizeof(x)/sizeof(x[0]))

BOOST_DATA_TEST_CASE( test_case_interface_07, samples1 * samples2 * samples3 * samples4, intval, str, val2, floatval )
{
    BOOST_TEST_CONTEXT("index7 " << index7) {
      BOOST_TEST( intval == samples1[index7/(sizeoftable(samples4)*sizeoftable(samples3)*samples2.size())] );
      BOOST_TEST( str == samples2[(index7/(sizeoftable(samples4)*sizeoftable(samples3)))%samples2.size()] );
      BOOST_TEST( val2 == samples3[(index7/sizeoftable(samples4))%sizeoftable(samples3)] );
      BOOST_TEST( floatval == samples4[index7%sizeoftable(samples4)] );
    }
    ++index7;
}

//____________________________________________________________________________//

static int index8 = 1;

struct SharedFixture {
    SharedFixture()
    : m_expected(index8++)
    {
    }

    int m_expected;
};

BOOST_DATA_TEST_CASE_F( SharedFixture, test_case_interface_08, data::make({1,2,3}) )
{
    BOOST_TEST( sample == m_expected );
}

//____________________________________________________________________________//

BOOST_DATA_TEST_CASE(test_case_interface_correct_file_line_declaration, samples2)
{
  boost::unit_test::test_case const& current_test_case = boost::unit_test::framework::current_test_case();
  BOOST_TEST(current_test_case.p_line_num == 136);
  BOOST_TEST(current_test_case.p_file_name == __FILE__);
}

//____________________________________________________________________________//
// ticket 13443

BOOST_DATA_TEST_CASE( 
  test_arity_above_9,
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ) ^
  data::make( { 1, 2, 3, 5 } ),
  sample1, sample2, sample3, sample4, sample5, sample6, sample7, sample8, sample9, sample10)
{
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_has_dataset )
{
    using t1 = decltype(data::make( 1 ));
    BOOST_TEST((data::monomorphic::has_dataset<t1>::value));
  
    BOOST_TEST((data::monomorphic::has_dataset<int, t1>::value));
    BOOST_TEST((!data::monomorphic::has_dataset<int, float>::value));
}

//____________________________________________________________________________//


static int index_fixture_setup_teardown = 0;

struct SharedFixtureSetupTeardown {
    SharedFixtureSetupTeardown()
    : m_expected(1 + index_fixture_setup_teardown)
    {}

    void setup() {
      m_expected *= m_expected;
    }

    void teardown() {
      index_fixture_setup_teardown++;
    }

    int m_expected;
};

BOOST_DATA_TEST_CASE_F( SharedFixtureSetupTeardown, test_case_interface_setup_teardown, data::make({0,1,2,3}) )
{
    BOOST_TEST( sample == index_fixture_setup_teardown );
    BOOST_TEST( m_expected == (1+sample)*(1+sample));
}

// EOF
