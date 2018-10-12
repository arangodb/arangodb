//----------------------------------------------------------------------------
/// @file test_parallel_stable_sort.cpp
/// @brief test program of the parallel_stable_sort algorithm
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
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <random>
#include <algorithm>
#include <boost/sort/parallel_stable_sort/parallel_stable_sort.hpp>
#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/test_tools.hpp>

namespace bss = boost::sort;


struct xk
{
    unsigned tail : 4;
    unsigned num : 28;
    xk ( uint32_t n =0 , uint32_t t =0): tail (t), num(n){};
    bool operator< (xk A) const { return (num < A.num); };
};
void test1()
{

    std::mt19937_64 my_rand(0);

    const uint32_t NMAX = 100000;

    std::vector<xk> V1, V2, V3;
    V1.reserve(NMAX);
    for (uint32_t i = 0; i < 8; ++i)
    {
        for (uint32_t k = 0; k < NMAX; ++k)
        {   uint32_t NM = my_rand();
            xk G;
            G.num = NM >> 3;
            G.tail = i;
            V1.push_back(G);
        };
    };
    V3 = V2 = V1;
    bss::parallel_stable_sort(V1.begin(), V1.end());
    std::stable_sort(V2.begin(), V2.end());
    bss::parallel_stable_sort(V3.begin(), V3.end(), 0);

    BOOST_CHECK(V1.size() == V2.size());
    for (uint32_t i = 0; i < V1.size(); ++i)
    {   BOOST_CHECK(V1[i].num == V2[i].num and V1[i].tail == V2[i].tail);
    };

    BOOST_CHECK(V3.size() == V2.size());
    for (uint32_t i = 0; i < V3.size(); ++i)
    {   BOOST_CHECK(V3[i].num == V2[i].num and V3[i].tail == V2[i].tail);
    };
};

void test2(void)
{
    const uint32_t NElem = 2000000;
    std::vector<uint64_t> V1;
    
    // ----------------- sorted elements ------------------------------------
    V1.clear();
    for (uint32_t i = 0; i < NElem; ++i)  V1.push_back(i);
    bss::parallel_stable_sort(V1.begin(), V1.end());
    for (unsigned i = 1; i < NElem; i++)
    {   BOOST_CHECK(V1[i - 1] <= V1[i]);
    };
    
    // ------------- reverse sorted elements --------------------------------
    V1.clear();
    for (uint32_t i = 0; i < NElem; ++i)   V1.push_back(NElem - i);
    bss::parallel_stable_sort(V1.begin(), V1.end());
    for (unsigned i = 1; i < NElem; i++)
    {   BOOST_CHECK(V1[i - 1] <= V1[i]);
    };
    
    // -------------------- equals elements -----------------------------------
    V1.clear();
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back(1000);
    bss::parallel_stable_sort(V1.begin(), V1.end());
    for (unsigned i = 1; i < NElem; i++)
    {   BOOST_CHECK(V1[i - 1] == V1[i]);
    };
};
void test3(void)
{
    const uint32_t NElem = 2000000;
    std::vector<uint64_t> V1,V2,V3;
    std::mt19937_64 my_rand(0);

    for (uint32_t i = 0; i < NElem; ++i) V1.push_back(my_rand() % NElem);
    V3 = V2 = V1;
    std::stable_sort (V2.begin(), V2.end());
    
    
    // --------------- unsorted elements 0 threads ----------------------------
    V3 = V1;
    bss::parallel_stable_sort(V3.begin(), V3.end(), 0);
    for (unsigned i = 0; i < NElem; i++)
    {   BOOST_CHECK(V3[i] == V2[i]);
    };
    
    // --------------- unsorted elements -------------------------------------
    V3 = V1;
    bss::parallel_stable_sort(V3.begin(), V3.end());
    for (unsigned i = 0; i < NElem; i++)
    {   BOOST_CHECK(V3[i] == V2[i]);
    };    
    
    // --------------- unsorted elements 100 threads ----------------------------
    V3 = V1;
    bss::parallel_stable_sort(V3.begin(), V3.end(), 100);
    for (unsigned i = 0; i < NElem; i++)
    {   BOOST_CHECK(V3[i] == V2[i]);
    };     
};

void test4(void)
{
    const uint32_t KMax = 66000;

    std::vector<uint64_t> K, M;
    std::mt19937_64 my_rand(0);

    for (uint32_t i = 0; i < KMax; ++i)
        K.push_back(my_rand());
    M = K;

    bss::parallel_stable_sort(K.begin(), K.end(), 300);

    std::stable_sort(M.begin(), M.end());
    for (unsigned i = 0; i < KMax; i++)
        BOOST_CHECK(M[i] == K[i]);
};

void test5 (void)
{
    typedef typename std::vector<xk>::iterator iter_t;
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
    bss::parallel_stable_sort( V.begin() , V.end());
    for ( uint32_t i =0 ; i < ( NELEM * 10); ++i)
    {   BOOST_CHECK ( V[i].num == (i / 10) and V[i].tail == (i %10) );
    };
}


int test_main(int, char *[])
{
    test1();
    test2();  
    test3();
    test4();
    test5();
    return 0;
};

