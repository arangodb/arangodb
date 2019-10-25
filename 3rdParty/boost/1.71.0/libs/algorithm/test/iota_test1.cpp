/* 
   Copyright (c) Marshall Clow 2011-2012.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <boost/config.hpp>
#include <boost/algorithm/cxx11/iota.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <list>

//  Test to make sure a sequence is "correctly formed"; i.e, ascending by one
template <typename Iterator, typename T>
BOOST_CXX14_CONSTEXPR bool test_iota_results ( Iterator first, Iterator last, T initial_value ) {
    if ( first == last ) return true;
    if ( initial_value != *first ) return false;
    Iterator prev = first;
    while ( ++first != last ) {
        if (( *first - *prev ) != 1 )
            return false;
        prev = first;
        }
    return true;
    }

    
template <typename Range, typename T>
BOOST_CXX14_CONSTEXPR bool test_iota_results ( const Range &r, T initial_value ) {
    return test_iota_results (boost::begin (r), boost::end (r), initial_value );
}

    
void test_ints () {
    std::vector<int> v;
    std::list<int> l;

    v.clear (); v.resize ( 10 );
    boost::algorithm::iota ( v.begin (), v.end (), 23 );
    BOOST_CHECK ( test_iota_results ( v.begin (), v.end (), 23 ));
    
    v.clear (); v.resize ( 19 );
    boost::algorithm::iota ( v, 18 );
    BOOST_CHECK ( test_iota_results ( v, 18 ));
    
    v.clear ();
    boost::algorithm::iota_n ( std::back_inserter(v), 99, 20 );
    BOOST_CHECK ( test_iota_results ( v, 99 ));
    
    v.clear ();
    boost::algorithm::iota_n ( std::back_inserter(v), 99, 0 );
    BOOST_CHECK ( v.size() == 0 );

/*
    l.clear (); l.reserve ( 5 );
    boost::algorithm::iota ( l.begin (), l.end (), 123 );
    BOOST_CHECK ( test_iota_results ( l.begin (), l.end (), 123 ));
    
    l.clear (); l.reserve ( 9 );
    boost::algorithm::iota ( l.begin (), l.end (), 87 );
    BOOST_CHECK ( test_iota_results ( l.begin (), l.end (), 87 ));
*/

    l.clear ();
    boost::algorithm::iota_n ( std::back_inserter(l), 99, 20 );
    BOOST_CHECK ( test_iota_results ( l, 99 ));
    
    l.clear ();
    boost::algorithm::iota_n ( std::front_inserter(l), 123, 20 );
    BOOST_CHECK ( test_iota_results ( l.rbegin (), l.rend (), 123 ));
    }
    
BOOST_CXX14_CONSTEXPR inline bool test_constexpr_iota() {
    bool res = true;
    int data[] = {0, 0, 0};
    boost::algorithm::iota(data, data, 1); // fill none
    res = (res && data[0] == 0);
    
    boost::algorithm::iota(data, data + 3, 1); // fill all
    res = (res && test_iota_results(data, data + 3, 1));
    
    return res;
    }
    
    
BOOST_CXX14_CONSTEXPR inline bool test_constexpr_iota_n() {
    bool res = true;
    int data[] = {0, 0, 0};
    boost::algorithm::iota_n(data, 1, 0); // fill none
    res = (res && data[0] == 0);
    
    boost::algorithm::iota_n(data, 1, 3); // fill all
    res = (res && test_iota_results(data, 1));
    
    return res;
    }


BOOST_AUTO_TEST_CASE( test_main )
{
  test_ints ();
  BOOST_CXX14_CONSTEXPR bool constexpr_iota_res = test_constexpr_iota ();
  BOOST_CHECK(constexpr_iota_res);
  BOOST_CXX14_CONSTEXPR bool constexpr_iota_n_res = test_constexpr_iota_n ();
  BOOST_CHECK(constexpr_iota_n_res);
}
