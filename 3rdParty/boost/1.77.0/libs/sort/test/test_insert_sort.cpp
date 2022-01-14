//----------------------------------------------------------------------------
/// @file test_insert_sort.cpp
/// @brief Test program of the insert_sort algorithm
///
/// @author Copyright (c) 2016 Francisco Jose Tapia (fjtapia@gmail.com )\n
///         Distributed under the Boost Software License, Version 1.0.\n
///         ( See accompanying file LICENSE_1_0.txt or copy at
///           http://www.boost.org/LICENSE_1_0.txt  )
/// @version 0.1
///
/// @remarks
//-----------------------------------------------------------------------------
#include <ciso646>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <boost/sort/insert_sort/insert_sort.hpp>
#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/test_tools.hpp>


using namespace boost::sort;
using namespace std;
using boost::sort::common::util::insert_sorted;

void test01 (void)
{
    unsigned A[] = {7,  4, 23, 15, 17, 2, 24, 13, 8,  3,  11,
                    16, 6, 14, 21, 5,  1, 12, 19, 22, 25, 8};

    std::less< unsigned > comp;
    // Insertion Sort  Unordered, not repeated
    insert_sort (&A[ 0 ], &A[ 22 ], comp);
    for (unsigned i = 0; i < 21; i++) {
        BOOST_CHECK (A[ i ] <= A[ i + 1 ]);
    };

    unsigned B[] = {1,  2,  3,  5,  6,  7,  8,  9,  10, 11, 12,
                    13, 14, 15, 17, 18, 19, 20, 21, 23, 24, 25};
    // Insertion Sort  Ordered, not repeated
    insert_sort (&B[ 0 ], &B[ 22 ], comp);
    for (unsigned i = 0; i < 21; i++) {
        BOOST_CHECK (B[ i ] <= B[ i + 1 ]);
    };

    unsigned C[] = {27, 26, 25, 23, 22, 21, 19, 18, 17, 16, 15,
                    14, 13, 11, 10, 9,  8,  7,  6,  5,  3,  2};
    // Insertion Sort reverse sorted, not repeated
    insert_sort (&C[ 0 ], &C[ 22 ], comp);
    for (unsigned i = 0; i < 21; i++) {
        BOOST_CHECK (C[ i ] <= C[ i + 1 ]);
    };

    unsigned D[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
                    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    // Insertion Sort  equal elements
    insert_sort (&D[ 0 ], &D[ 22 ], comp);
    for (unsigned i = 0; i < 21; i++) {
        BOOST_CHECK (D[ i ] <= D[ i + 1 ]);
    };

    // Insertion Sort  100 random elements
    unsigned F[ 100 ];
    for (unsigned i = 0; i < 100; i++) F[ i ] = rand ( ) % 1000;
    insert_sort (&F[ 0 ], &F[ 100 ], comp);
    for (unsigned i = 0; i < 99; i++) {
        BOOST_CHECK (F[ i ] <= F[ i + 1 ]);
    };

    const unsigned NG = 10000;
    // Insertion Sort  "<<NG<<" random elements
    unsigned G[ NG ];
    for (unsigned i = 0; i < NG; i++) G[ i ] = rand ( ) % 1000;
    insert_sort (&G[ 0 ], &G[ NG ], comp);
    for (unsigned i = 0; i < NG - 1; i++) {
        BOOST_CHECK (G[ i ] <= G[ i + 1 ]);
    };
};
void test02 (void)
{
    typedef typename std::vector< uint64_t >::iterator iter_t;
    const uint32_t NELEM = 6667;
    std::vector< uint64_t > A;
    std::less< uint64_t > comp;

    for (uint32_t i = 0; i < 1000; ++i) A.push_back (0);
    for (uint32_t i = 0; i < NELEM; ++i) A.push_back (NELEM - i);
    for (uint32_t i = 0; i < 1000; ++i) A.push_back (0);

    insert_sort (A.begin ( ) + 1000, A.begin ( ) + (1000 + NELEM), comp);

    for (iter_t it = A.begin ( ) + 1000; it != A.begin ( ) + (1000 + NELEM);
         ++it)
    {
        BOOST_CHECK ((*(it - 1)) <= (*it));
    };
    BOOST_CHECK (A[ 998 ] == 0 and A[ 999 ] == 0 and A[ 1000 + NELEM ] == 0 and
                 A[ 1001 + NELEM ] == 0);


    A.clear ( );
    for (uint32_t i = 0; i < 1000; ++i) A.push_back (999999999);
    for (uint32_t i = 0; i < NELEM; ++i) A.push_back (NELEM - i);
    for (uint32_t i = 0; i < 1000; ++i) A.push_back (999999999);

    insert_sort (A.begin ( ) + 1000, A.begin ( ) + (1000 + NELEM), comp);

    for (iter_t it = A.begin ( ) + 1001; it != A.begin ( ) + (1000 + NELEM);
         ++it)
    {
        BOOST_CHECK ((*(it - 1)) <= (*it));
    };
    BOOST_CHECK (A[ 998 ] == 999999999 and A[ 999 ] == 999999999 and
                 A[ 1000 + NELEM ] == 999999999 and
                 A[ 1001 + NELEM ] == 999999999);
};


void test03 ( void)
{
    std::vector<uint32_t> V {1,3,5,2,4};
    std::less<uint32_t> comp ;
    uint32_t aux[10] ;

    insert_sorted ( V.begin() , V.begin()+3, V.end(), comp, aux);
    //insert_partial_sort ( V.begin() , V.begin()+3, V.end() , comp);
    for ( uint32_t i =0 ; i < V.size() ; ++i)
        std::cout<<V[i]<<", ";
    std::cout<<std::endl;

    V ={3,5,7,9,11,13,15,17,19,8,6,10,4,2};
    insert_sorted ( V.begin() , V.begin()+9, V.end() , comp, aux);
    //insert_partial_sort ( V.begin() , V.begin()+9, V.end() , comp);
    for ( uint32_t i =0 ; i < V.size() ; ++i)
        std::cout<<V[i]<<", ";
    std::cout<<std::endl;

    V ={13,15,17,19,21,23,35,27,29,8,6,10,4,2};
    insert_sorted ( V.begin() , V.begin()+9, V.end() , comp, aux);
    //insert_partial_sort ( V.begin() , V.begin()+9, V.end() , comp);
    for ( uint32_t i =0 ; i < V.size() ; ++i)
        std::cout<<V[i]<<", ";
    std::cout<<std::endl;

    V ={3,5,7,9,11,13,15,17,19,28,26,30,24,22};
    insert_sorted ( V.begin() , V.begin()+9, V.end() , comp, aux);
    //insert_partial_sort ( V.begin() , V.begin()+9, V.end() , comp);
    for ( uint32_t i =0 ; i < V.size() ; ++i)
        std::cout<<V[i]<<", ";
    std::cout<<std::endl;



}
int test_main (int, char *[])
{
    test01 ( );
    test02 ( );
    test03 ( );
    return 0;
}
