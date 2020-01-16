/* 
   Copyright (c) Marshall Clow 2011-2012.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <boost/config.hpp>
#include <boost/algorithm/cxx11/copy_n.hpp>
#include <boost/algorithm/cxx14/equal.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <string>
#include <iostream>
#include <vector>
#include <list>

namespace ba = boost::algorithm;
// namespace ba = boost;

BOOST_CXX14_CONSTEXPR bool is_zero( int v ) { return v == 0; }

template <typename Container>
void test_sequence ( Container const &c ) {

    typedef typename Container::value_type value_type;
    std::vector<value_type> v;
    
//  Copy zero elements
    v.clear ();
    ba::copy_n ( c.begin (), 0, back_inserter ( v ));
    BOOST_CHECK ( v.size () == 0 );
    ba::copy_n ( c.begin (), 0U, back_inserter ( v ));
    BOOST_CHECK ( v.size () == 0 );

    if ( c.size () > 0 ) {  
    //  Just one element
        v.clear ();
        ba::copy_n ( c.begin (), 1, back_inserter ( v ));
        BOOST_CHECK ( v.size () == 1 );
        BOOST_CHECK ( v[0] == *c.begin ());
        
        v.clear ();
        ba::copy_n ( c.begin (), 1U, back_inserter ( v ));
        BOOST_CHECK ( v.size () == 1 );
        BOOST_CHECK ( v[0] == *c.begin ());

    //  Half the elements
        v.clear ();
        ba::copy_n ( c.begin (), c.size () / 2, back_inserter ( v ));
        BOOST_CHECK ( v.size () == c.size () / 2);
        BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));

    //  Half the elements + 1
        v.clear ();
        ba::copy_n ( c.begin (), c.size () / 2 + 1, back_inserter ( v ));
        BOOST_CHECK ( v.size () == c.size () / 2 + 1 );
        BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));
    
    //  All the elements
        v.clear ();
        ba::copy_n ( c.begin (), c.size (), back_inserter ( v ));
        BOOST_CHECK ( v.size () == c.size ());
        BOOST_CHECK ( std::equal ( v.begin (), v.end (), c.begin ()));
        }
    }


BOOST_CXX14_CONSTEXPR inline bool test_constexpr() {
    const size_t sz = 64;
    int in_data[sz] = {0};
    bool res = true;
    
    const int* from = in_data;
    const int* to = in_data + sz;
    
    int out_data[sz] = {0};
    int* out = out_data;
    
    out = ba::copy_n ( from, 0, out ); // Copy none
    res = (res && out == out_data && ba::all_of(out, out + sz, is_zero));

    out = ba::copy_n ( from, sz, out ); // Copy all
    res = (res && out == out_data + sz
           && ba::equal( input_iterator<const int *>(out_data),  input_iterator<const int *>(out_data + sz), 
                         input_iterator<const int *>(from), input_iterator<const int *>(to)));
    
    return res;
    }
    
    
void test_sequence1 () {
    std::vector<int> v;
    for ( int i = 5; i < 15; ++i )
        v.push_back ( i );
    test_sequence  ( v );
    
    BOOST_CXX14_CONSTEXPR bool constexpr_res = test_constexpr();
    BOOST_CHECK(constexpr_res);
    
    std::list<int> l;
    for ( int i = 25; i > 15; --i )
        l.push_back ( i );
    test_sequence  ( l );   
    }


BOOST_AUTO_TEST_CASE( test_main )
{
  test_sequence1 ();
}
