//  (C) Copyright Gennadiy Rozental 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// @brief tests collection comparison implementation
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE Test collection`s comparisons
#include <boost/test/unit_test.hpp>
namespace tt = boost::test_tools;
namespace ut = boost::unit_test;

BOOST_TEST_SPECIALIZED_COLLECTION_COMPARE(std::vector<int>)

#define VALIDATE_OP( op )                           \
{                                                   \
    BOOST_TEST_INFO( "validating operator " #op );  \
    bool expected = (c1 op c2);                     \
    auto const& res = (tt::assertion::seed()->* c1 op c2).evaluate();      \
    BOOST_TEST( expected == !!res );                \
}                                                   \
/**/

template<typename Col>
void
validate_comparisons(Col const& c1, Col const& c2 )
{
    VALIDATE_OP( == )
    VALIDATE_OP( != )
    VALIDATE_OP( < )
    VALIDATE_OP( > )
    VALIDATE_OP( <= )
    VALIDATE_OP( >= )
}

BOOST_AUTO_TEST_CASE( test_against_overloaded_comp_op )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};
    std::vector<int> c{1, 2, 3};
    std::vector<int> d{1, 2, 3, 4};

    BOOST_TEST_CONTEXT( "validating comparisons of a and b" )
        validate_comparisons(a, b);
    BOOST_TEST_CONTEXT( "validating comparisons of a and c" )
        validate_comparisons(a, c);
    BOOST_TEST_CONTEXT( "validating comparisons of a and d" )
        validate_comparisons(a, d);
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_per_element_eq, * ut::expected_failures(2) )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( a == b, tt::per_element() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_per_element_ne, * ut::expected_failures(1) )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( a != b, tt::per_element() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_per_element_lt, * ut::expected_failures(2) )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( a < b, tt::per_element() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_per_element_ge, * ut::expected_failures(1) )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( b >= a, tt::per_element() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_lexicographic_lt )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( a < b, tt::lexicographic() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_lexicographic_le )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( a <= b, tt::lexicographic() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_lexicographic_gt )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( b > a, tt::lexicographic() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_lexicographic_ge )
{
    std::vector<int> a{1, 2, 3};
    std::vector<int> b{1, 3, 2};

    BOOST_TEST( b >= a, tt::lexicographic() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_collection_of_collection_comp )
{
    BOOST_TEST( std::string("abc") == std::string("abc") );
}

//____________________________________________________________________________//

// this one does not have const_iterator nor a size, but should be forward iterable
// and possible to use in the collection comparison
struct fwd_iterable_custom {
  typedef std::vector<int>::const_iterator custom_iterator; // named "exotic" on purpose

  custom_iterator begin() const { return values.begin(); }
  custom_iterator end() const { return values.end(); }

#if !defined(BOOST_MSVC) || (BOOST_MSVC_FULL_VER > 180040629)
#define MY_TEST_HAS_INIT_LIST
  // this does not work on VC++ 2013 update 5
  fwd_iterable_custom(std::initializer_list<int> ilist) : values{ilist}
  {}
#else
  fwd_iterable_custom(int v1, int v2, int v3) {
    values.push_back(v1);
    values.push_back(v2);
    values.push_back(v3);
  }
#endif
private:
  std::vector<int> values;
};

BOOST_AUTO_TEST_CASE( test_collection_requirement_type )
{
#ifdef MY_TEST_HAS_INIT_LIST
    fwd_iterable_custom a{3,4,5};
    fwd_iterable_custom b{3,4,6};
    fwd_iterable_custom c{3,4,5};
#else
    fwd_iterable_custom a(3,4,5);
    fwd_iterable_custom b(3,4,6);
    fwd_iterable_custom c(3,4,5);
#endif

    BOOST_TEST( a == a, tt::per_element() );
    //BOOST_TEST( a != b, tt::per_element() );
    BOOST_TEST( a == c, tt::per_element() );

    BOOST_TEST( a < b, tt::lexicographic() );
    BOOST_TEST( a <= c, tt::lexicographic() );
    BOOST_TEST( b > c, tt::lexicographic() );

    BOOST_TEST( a <= b, tt::per_element() );
    BOOST_TEST( a <= c, tt::per_element() );
}

// EOF
