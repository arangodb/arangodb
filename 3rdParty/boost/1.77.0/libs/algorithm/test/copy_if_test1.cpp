/* 
   Copyright (c) Marshall Clow 2012.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <boost/config.hpp>
#include <boost/algorithm/cxx11/copy_if.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <string>
#include <iostream>
#include <vector>
#include <list>

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx14/equal.hpp>
#include <boost/algorithm/cxx11/none_of.hpp>

namespace ba = boost::algorithm;
// namespace ba = boost;

BOOST_CXX14_CONSTEXPR bool is_true  ( int v ) { return true; }
BOOST_CXX14_CONSTEXPR bool is_false ( int v ) { return false; }
BOOST_CXX14_CONSTEXPR bool is_even  ( int v ) { return v % 2 == 0; }
BOOST_CXX14_CONSTEXPR bool is_odd   ( int v ) { return v % 2 == 1; }
BOOST_CXX14_CONSTEXPR bool is_zero  ( int v ) { return v == 0; }


template <typename Container>
void test_copy_if ( Container const &c ) {

    typedef typename Container::value_type value_type;
    std::vector<value_type> v;
    
//  None of the elements
    v.clear ();
    ba::copy_if ( c.begin (), c.end (), back_inserter ( v ), is_false);
    BOOST_CHECK ( v.size () == 0 );

    v.clear ();
    ba::copy_if ( c, back_inserter ( v ), is_false);
    BOOST_CHECK ( v.size () == 0 );

//  All the elements
    v.clear ();
    ba::copy_if ( c.begin (), c.end (), back_inserter ( v ), is_true);
    BOOST_CHECK ( v.size () == c.size ());
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

    v.clear ();
    ba::copy_if ( c, back_inserter ( v ), is_true);
    BOOST_CHECK ( v.size () == c.size ());
    BOOST_CHECK ( v.size () == c.size ());
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

//  Some of the elements
    v.clear ();
    ba::copy_if ( c.begin (), c.end (), back_inserter ( v ), is_even );
    BOOST_CHECK ( v.size () == (size_t) std::count_if ( c.begin (), c.end (), is_even ));
    BOOST_CHECK ( ba::all_of ( v.begin (), v.end (), is_even ));

    v.clear ();
    ba::copy_if ( c, back_inserter ( v ), is_even );
    BOOST_CHECK ( v.size () == (size_t) std::count_if ( c.begin (), c.end (), is_even ));
    BOOST_CHECK ( ba::all_of ( v.begin (), v.end (), is_even ));
    }


template <typename Container>
void test_copy_while ( Container const &c ) {

    typedef typename Container::value_type value_type;
    typename Container::const_iterator it;
    std::vector<value_type> v;
    
//  None of the elements
    v.clear ();
    ba::copy_while ( c.begin (), c.end (), back_inserter ( v ), is_false);
    BOOST_CHECK ( v.size () == 0 );
    
    v.clear ();
    ba::copy_while ( c, back_inserter ( v ), is_false);
    BOOST_CHECK ( v.size () == 0 );

//  All the elements
    v.clear ();
    ba::copy_while ( c.begin (), c.end (), back_inserter ( v ), is_true);
    BOOST_CHECK ( v.size () == c.size ());
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

    v.clear ();
    ba::copy_while ( c, back_inserter ( v ), is_true);
    BOOST_CHECK ( v.size () == c.size ());
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

//  Some of the elements
    v.clear ();
    it = ba::copy_while ( c.begin (), c.end (), back_inserter ( v ), is_even ).first;
    BOOST_CHECK ( v.size () == (size_t) std::distance ( c.begin (), it ));
    BOOST_CHECK ( it == c.end () || !is_even ( *it ));
    BOOST_CHECK ( ba::all_of ( v.begin (), v.end (), is_even ));
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

    v.clear ();
    it = ba::copy_while ( c, back_inserter ( v ), is_even ).first;
    BOOST_CHECK ( v.size () == (size_t) std::distance ( c.begin (), it ));
    BOOST_CHECK ( it == c.end () || !is_even ( *it ));
    BOOST_CHECK ( ba::all_of ( v.begin (), v.end (), is_even ));
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));
    }

template <typename Container>
void test_copy_until ( Container const &c ) {

    typedef typename Container::value_type value_type;
    typename Container::const_iterator it;
    std::vector<value_type> v;
    
//  None of the elements
    v.clear ();
    ba::copy_until ( c.begin (), c.end (), back_inserter ( v ), is_true);
    BOOST_CHECK ( v.size () == 0 );

    v.clear ();
    ba::copy_until ( c, back_inserter ( v ), is_true);
    BOOST_CHECK ( v.size () == 0 );

//  All the elements
    v.clear ();
    ba::copy_until ( c.begin (), c.end (), back_inserter ( v ), is_false);
    BOOST_CHECK ( v.size () == c.size ());
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

    v.clear ();
    ba::copy_until ( c, back_inserter ( v ), is_false);
    BOOST_CHECK ( v.size () == c.size ());
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

//  Some of the elements
    v.clear ();
    it = ba::copy_until ( c.begin (), c.end (), back_inserter ( v ), is_even ).first;
    BOOST_CHECK ( v.size () == (size_t) std::distance ( c.begin (), it ));
    BOOST_CHECK ( it == c.end () || is_even ( *it ));
    BOOST_CHECK ( ba::none_of ( v.begin (), v.end (), is_even ));
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

    v.clear ();
    it = ba::copy_until ( c, back_inserter ( v ), is_even ).first;
    BOOST_CHECK ( v.size () == (size_t) std::distance ( c.begin (), it ));
    BOOST_CHECK ( it == c.end () || is_even ( *it ));
    BOOST_CHECK ( ba::none_of ( v.begin (), v.end (), is_even ));
    BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));
    }


BOOST_CXX14_CONSTEXPR inline bool constexpr_test_copy_if() {
    const int sz = 64;
    int in_data[sz] = {0};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + sz;
    
    int out_data[sz] = {0};
    int* out = out_data;
    out = ba::copy_if ( from, to, out, is_false ); // copy none
    res = (res && out == out_data);
    
    out = ba::copy_if ( from, to, out, is_true ); // copy all
    res = (res && out == out_data + sz
           && ba::equal( input_iterator<const int *>(out_data),  input_iterator<const int *>(out_data + sz), 
                         input_iterator<const int *>(from), input_iterator<const int *>(to)));
    
    return res;
    }

BOOST_CXX14_CONSTEXPR inline bool constexpr_test_copy_while() {
    const int sz = 64;
    int in_data[sz] = {0};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + sz;
    
    int out_data[sz] = {0};
    int* out = out_data;
    out = ba::copy_while ( from, to, out, is_false ).second; // copy none
    res = (res && out == out_data && ba::all_of(out, out + sz, is_zero));
    
    out = ba::copy_while ( from, to, out, is_true ).second; // copy all
    res = (res && out == out_data + sz
           && ba::equal( input_iterator<const int *>(out_data),  input_iterator<const int *>(out_data + sz), 
                         input_iterator<const int *>(from), input_iterator<const int *>(to)));
    
    return res;
    }

BOOST_CXX14_CONSTEXPR inline bool constexpr_test_copy_until() {
    const int sz = 64;
    int in_data[sz] = {0};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + sz;
    
    int out_data[sz] = {0};
    int* out = out_data;
    out = ba::copy_until ( from, to, out, is_true ).second; // copy none
    res = (res && out == out_data && ba::all_of(out, out + sz, is_zero));
    
    out = ba::copy_until ( from, to, out, is_false ).second; // copy all
    res = (res && out == out_data + sz
           && ba::equal( input_iterator<const int *>(out_data),  input_iterator<const int *>(out_data + sz), 
                         input_iterator<const int *>(from), input_iterator<const int *>(to)));
    
    return res;
    }
    
    
void test_sequence1 () {
    std::vector<int> v;
    for ( int i = 5; i < 15; ++i )
        v.push_back ( i );
    test_copy_if ( v );
    test_copy_while ( v );
    test_copy_until ( v );
    
    BOOST_CXX14_CONSTEXPR bool constexpr_res_if = constexpr_test_copy_if();
    BOOST_CHECK ( constexpr_res_if );
    BOOST_CXX14_CONSTEXPR bool constexpr_res_while = constexpr_test_copy_while();
    BOOST_CHECK ( constexpr_res_while );
    BOOST_CXX14_CONSTEXPR bool constexpr_res_until = constexpr_test_copy_until();
    BOOST_CHECK ( constexpr_res_until );
    
    std::list<int> l;
    for ( int i = 25; i > 15; --i )
        l.push_back ( i );
    test_copy_if ( l );
    test_copy_while ( l );
    test_copy_until ( l );
    }


BOOST_AUTO_TEST_CASE( test_main )
{
  test_sequence1 ();
}
