/* 
   Copyright (c) Marshall Clow 2013.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <boost/config.hpp>
#include <boost/algorithm/cxx14/mismatch.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

template <typename T>
BOOST_CXX14_CONSTEXPR bool eq ( const T& a, const T& b ) { return a == b; }

template <typename T>
BOOST_CXX14_CONSTEXPR bool never_eq ( const T&, const T& ) { return false; }

namespace ba = boost::algorithm;

template <typename Iter1, typename Iter2>
BOOST_CXX14_CONSTEXPR bool iter_eq ( std::pair<Iter1, Iter2> pr, Iter1 first, Iter2 second ) {
    return pr.first == first && pr.second == second;
    }

void test_mismatch ()
{
//  Note: The literal values here are tested against directly, careful if you change them:
    BOOST_CXX14_CONSTEXPR int num[] = { 1, 1, 2, 3, 5 };
    const int sz = sizeof (num)/sizeof(num[0]);
    
    
//  No mismatch for empty sequences
    BOOST_CHECK ( iter_eq ( 
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num), 
                       input_iterator<const int *>(num),     input_iterator<const int *>(num)),
                input_iterator<const int *>(num), input_iterator<const int *>(num)));
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num), 
                       input_iterator<const int *>(num),     input_iterator<const int *>(num),
                       never_eq<int> ),
                input_iterator<const int *>(num), input_iterator<const int *>(num)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num), 
                       random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num),
                       never_eq<int> ),
                random_access_iterator<const int *>(num), random_access_iterator<const int *>(num)));
                              
//  Empty vs. non-empty mismatch immediately
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num), 
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + 1)),
                input_iterator<const int *>(num),     input_iterator<const int *>(num)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 2), 
                       input_iterator<const int *>(num),     input_iterator<const int *>(num)),
                input_iterator<const int *>(num + 1), input_iterator<const int *>(num)));
                    
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( random_access_iterator<const int *>(num + 1), random_access_iterator<const int *>(num + 2), 
                       random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num)),
                random_access_iterator<const int *>(num + 1), random_access_iterator<const int *>(num)));

//  Single element sequences are equal if they contain the same value
    BOOST_CHECK ( iter_eq ( 
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + 1)),
                input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 1)));
                       
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       eq<int> ),
                input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 1)));
                    
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num + 1),
                       random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num + 1),
                       eq<int> ),
                random_access_iterator<const int *>(num + 1), random_access_iterator<const int *>(num + 1)));


    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       never_eq<int> ),
               input_iterator<const int *>(num),     input_iterator<const int *>(num)));
               
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num + 1),
                       random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num + 1),
                       never_eq<int> ),
               random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 2)),
               input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 2)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 2),
                       eq<int> ),
               input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 2)));

    BOOST_CHECK ( iter_eq (
            ba::mismatch ( input_iterator<const int *>(num + 2), input_iterator<const int *>(num + 3), 
                           input_iterator<const int *>(num),     input_iterator<const int *>(num + 1)),
                           input_iterator<const int *>(num + 2), input_iterator<const int *>(num)));
                           
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num + 2), input_iterator<const int *>(num + 3),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       eq<int> ),
               input_iterator<const int *>(num + 2), input_iterator<const int *>(num)));
                       
                              
                              
//  Identical long sequences are equal. 
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + sz)),
            input_iterator<const int *>(num + sz), input_iterator<const int *>(num + sz)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       eq<int> ),
            input_iterator<const int *>(num + sz), input_iterator<const int *>(num + sz)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       never_eq<int> ),
            input_iterator<const int *>(num),     input_iterator<const int *>(num)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),             input_iterator<const int *>(num + sz),
                       random_access_iterator<const int *>(num),     random_access_iterator<const int *>(num + sz),
                       never_eq<int> ),
            input_iterator<const int *>(num),     random_access_iterator<const int *>(num)));

//  Different sequences are different
    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num + 1), input_iterator<const int *>(num + sz),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + sz)),
            input_iterator<const int *>(num + 2), input_iterator<const int *>(num + 1)));

    BOOST_CHECK ( iter_eq (
        ba::mismatch ( input_iterator<const int *>(num + 1), input_iterator<const int *>(num + sz),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       eq<int> ),
            input_iterator<const int *>(num + 2), input_iterator<const int *>(num + 1)));
            
//  Checks constexpr
    BOOST_CXX14_CONSTEXPR bool res = (
//  No mismatch for empty
        iter_eq ( 
            ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num), 
                           input_iterator<const int *>(num),     input_iterator<const int *>(num)),
                input_iterator<const int *>(num), input_iterator<const int *>(num))
//  Empty vs. non-empty mismatch immediately  
        && iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num), 
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + 1)),
                input_iterator<const int *>(num),     input_iterator<const int *>(num))
//  Single element sequences are equal if they contain the same value                
        && iter_eq ( 
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + 1),
                       eq<int>),
                input_iterator<const int *>(num + 1), input_iterator<const int *>(num + 1))
//  Identical long sequences are equal.
        && iter_eq (
        ba::mismatch ( input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       input_iterator<const int *>(num),     input_iterator<const int *>(num + sz),
                       eq<int> ),
                input_iterator<const int *>(num + sz), input_iterator<const int *>(num + sz))
        );
    
    BOOST_CHECK ( res );
}


BOOST_AUTO_TEST_CASE( test_main )
{
  test_mismatch ();
}
