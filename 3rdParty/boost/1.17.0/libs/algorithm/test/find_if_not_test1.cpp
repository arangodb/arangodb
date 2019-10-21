/* 
   Copyright (c) Marshall Clow 2011-2012.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <iostream>

#include <boost/config.hpp>
#include <boost/algorithm/cxx11/find_if_not.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>
#include <list>

namespace ba = boost::algorithm;
// namespace ba = boost;

BOOST_CXX14_CONSTEXPR bool is_true  ( int v ) { return true; }
BOOST_CXX14_CONSTEXPR bool is_false ( int v ) { return false; }
BOOST_CXX14_CONSTEXPR bool is_not_three ( int v ) { return v != 3; }

BOOST_CXX14_CONSTEXPR bool check_constexpr() {
    int in_data[] = {1, 2, 3, 4, 5};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + 5;
    
    const int* start = ba::find_if_not (from, to, is_false); // stops on first
    res = (res && start == from);
    
    const int* end = ba::find_if_not(from, to, is_true); // stops on the end
    res = (res && end == to);
    
    const int* three = ba::find_if_not(from, to, is_not_three); // stops on third element
    res = (res && three == in_data + 2);
    
    return res;
}

template <typename Container>
typename Container::iterator offset_to_iter ( Container &v, int offset ) {
    typename Container::iterator retval;
    
    if ( offset >= 0 ) {
        retval = v.begin ();
        std::advance ( retval, offset );
        }
    else {
        retval = v.end ();
        std::advance ( retval, offset + 1 );
        }
    return retval;
    }

template <typename Container, typename Predicate>
void test_sequence ( Container &v, Predicate comp, int expected ) {
    typename Container::iterator res, exp;
    
    res = ba::find_if_not ( v.begin (), v.end (), comp );
    exp = offset_to_iter ( v, expected );
    std::cout << "Expected(1): " << std::distance ( v.begin (), exp ) 
              <<       ", got: " << std::distance ( v.begin (), res ) << std::endl;
    BOOST_CHECK ( exp == res );
    }

template <typename T>
struct less_than {
public:
    less_than ( T foo ) : val ( foo ) {}
    less_than ( const less_than &rhs ) : val ( rhs.val ) {}

    bool operator () ( const T &v ) const { return v < val; }
private:
    less_than ();
    less_than operator = ( const less_than &rhs );
    T val;
    };


void test_sequence1 () {
    std::vector<int> v;
    
    v.clear ();
    for ( int i = 5; i < 15; ++i )
        v.push_back ( i );
    test_sequence ( v, less_than<int>(3),  0 ); // no elements
    test_sequence ( v, less_than<int>(6),  1 );    // only the first element
    test_sequence ( v, less_than<int>(10), 5 );
    test_sequence ( v, less_than<int>(99), -1 );   // all elements satisfy 

//  With bidirectional iterators.
    std::list<int> l;
    for ( int i = 5; i < 15; ++i )
        l.push_back ( i );
    test_sequence ( l, less_than<int>(3),  0 ); // no elements
    test_sequence ( l, less_than<int>(6),  1 );    // only the first element
    test_sequence ( l, less_than<int>(10), 5 );
    test_sequence ( l, less_than<int>(99), -1 );   // all elements satisfy 

    }


BOOST_AUTO_TEST_CASE( test_main )
{
  test_sequence1 ();
}
