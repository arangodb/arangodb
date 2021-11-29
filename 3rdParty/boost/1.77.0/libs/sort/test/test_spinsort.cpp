//----------------------------------------------------------------------------
/// @file test_spinsort.cpp
/// @brief test program of the spinsort algorithm
///
/// @author Copyright (c) 2016 Francisco José Tapia (fjtapia@gmail.com )\n
///         Distributed under the Boost Software License, Version 1.0.\n
///         ( See accompanying file LICENSE_1_0.txt or copy at
///           http://www.boost.org/LICENSE_1_0.txt  )
/// @version 0.1
///
/// @remarks
//-----------------------------------------------------------------------------
#include <ciso646>
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <random>
#include <boost/sort/spinsort/spinsort.hpp>
#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/test_tools.hpp>


using namespace boost::sort;
using spin_detail::check_stable_sort;
using spin_detail::range_sort;
using common::range;

void test1 ( );
void test2 ( );
void test3 ( );
void test4 ( );

//---------------- stability test -----------------------------------
struct xk
{
    unsigned tail : 4;
    unsigned num : 28;
    xk ( uint32_t n =0 , uint32_t t =0): tail (t), num(n){};
    bool operator< (xk A) const { return (num < A.num); };
};

void test1 ( )
{
    typedef std::less< xk > compare_t;
    std::mt19937_64 my_rand (0);

    const uint32_t NMAX = 100000;

    std::vector< xk > V1, V2;
    V1.reserve (NMAX);
    for (uint32_t i = 0; i < 8; ++i) {
        for (uint32_t k = 0; k < NMAX; ++k) {
            uint32_t NM = my_rand ( );
            xk G;
            G.num = NM >> 3;
            G.tail = i;
            V1.push_back (G);
        };
    };
    V2 = V1;
    spinsort (V1.begin ( ), V1.end ( ), compare_t ( ));
    std::stable_sort (V2.begin ( ), V2.end ( ));

    BOOST_CHECK (V1.size ( ) == V2.size ( ));
    for (uint32_t i = 0; i < V1.size ( ); ++i) {
        BOOST_CHECK (V1[ i ].num == V2[ i ].num and
                     V1[ i ].tail == V2[ i ].tail);
    };
};

void test2 (void)
{
    typedef std::less< uint64_t > compare_t;

    const uint32_t NElem = 100000;
    std::vector< uint64_t > V1,V2;
    std::mt19937_64 my_rand (0);
    compare_t comp;

    // ------------------------ random elements -------------------------------
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (my_rand ( ) % NElem);
    V2 = V1;
    spinsort (V1.begin ( ), V1.end ( ), comp);
    std::stable_sort (V2.begin ( ), V2.end ( ), comp);
    for (unsigned i = 0; i < NElem; i++) {
        BOOST_CHECK (V2[ i ] == V1[ i ]);
    };

    // --------------------------- sorted elements ----------------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (i);
    spinsort (V1.begin ( ), V1.end ( ), comp);
    for (unsigned i = 1; i < NElem; i++) {
        BOOST_CHECK (V1[ i - 1 ] <= V1[ i ]);
    };

    //-------------------------- reverse sorted elements ----------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (NElem - i);
    spinsort (V1.begin ( ), V1.end ( ), comp);
    for (unsigned i = 1; i < NElem; i++) {
        BOOST_CHECK (V1[ i - 1 ] <= V1[ i ]);
    };

    //---------------------------- equal elements ----------------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (1000);
    spinsort (V1.begin ( ), V1.end ( ), comp);
    for (unsigned i = 1; i < NElem; i++) {
        BOOST_CHECK (V1[ i - 1 ] == V1[ i ]);
    };
};


void test3 (void)
{
    typedef typename std::vector<uint64_t>::iterator    iter_t ;
    typedef range<iter_t>                               range_it ;
    std::less<uint64_t> comp ;

    std::vector<uint64_t> V = { 1,  3,  5,  7,  9, 11, 13, 15, 17, 19,
                               21, 23, 25, 27, 29, 31, 33, 35, 37, 39,
                               41, 43, 45, 47, 49, 51, 53, 55, 57, 59,
                               14,  2,  4,  6,  8, 10, 12, 16, 18, 20};
    range_it rdata (V.begin() , V.end());
    std::vector<uint64_t> aux (40,0 );
    range_it raux ( aux.begin() , aux.end());

    check_stable_sort ( rdata, raux, comp );
    for ( uint32_t i =0 ; i < V.size() ; ++i)
        std::cout<<V[i]<<", ";
    std::cout<<std::endl;

    V = {59, 57, 55, 53, 51, 49, 47, 45, 43, 41,
         39, 37, 35, 33, 31, 29, 27, 25, 23, 21,
         19, 17, 15, 13, 11,  9,  7,  5,  3,  1,
         14,  2,  6, 16, 18, 20, 12,  4,  8, 10};
    rdata = range_it (V.begin() , V.end());
    aux.assign (40,0);
    raux = range_it ( aux.begin() , aux.end());
    check_stable_sort ( rdata, raux, comp );
    for ( uint32_t i =0 ; i < V.size() ; ++i)
        std::cout<<V[i]<<", ";
    std::cout<<std::endl;

}
void test4 (void)
{
    typedef typename std::vector<xk>::iterator  iter_t;
    typedef std::less<xk>        compare_t;
    std::mt19937 my_rand (0);
    std::vector<xk> V ;
    const uint32_t NELEM = 100000;
    V.reserve(NELEM * 10);


    for (uint32_t k =0 ; k < 10 ; ++k)
    {   for ( uint32_t i =0 ; i < NELEM ; ++i)
        {   V.emplace_back(i , k);
        };
        iter_t first = V.begin() + (k * NELEM);
        iter_t last = first + NELEM ;
        std::shuffle( first, last, my_rand);
    };
    spinsort( V.begin() , V.end(), compare_t());
    for ( uint32_t i =0 ; i < ( NELEM * 10); ++i)
    {   BOOST_CHECK ( V[i].num == (i / 10) and V[i].tail == (i %10) );
    };
}
int test_main (int, char *[])
{
    test1 ( );
    test2 ( );
    test3 ( );
    test4 ( );
    return 0;
};
