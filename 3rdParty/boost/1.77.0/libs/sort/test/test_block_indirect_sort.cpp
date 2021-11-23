//----------------------------------------------------------------------------
/// @file test_block_indirect_sort.cpp
/// @brief Test program of the block_indirect_sort algorithm
///
/// @author Copyright (c) 2016 Francisco Jose Tapia (fjtapia@gmail.com )\n
///         Distributed under the Boost Software License, Version 1.0.\n
///         ( See accompanying file LICENSE_1_0.txt or copy at
///           http://www.boost.org/LICENSE_1_0.txt  )
/// @version 0.1
///
/// @remarks
//-----------------------------------------------------------------------------
#include <algorithm>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <ciso646>
#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/sort/sort.hpp>


namespace bsc = boost::sort::common;
namespace bsp = boost::sort;
using boost::sort::block_indirect_sort;
using bsc::range;

// ------------------- vector with 64 bits random numbers --------------------
std::vector< uint64_t > Vrandom;
const uint64_t NELEM = 2000000;

void test1 (void)
{
    const uint32_t NElem = 500000;
    std::vector< uint64_t > V1;
    V1.reserve (NElem);
    
    //------------------ sorted elements  4 threads --------------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (i);

    block_indirect_sort ( V1.begin ( ), V1.end ( ), 4);
    for (unsigned i = 1; i < NElem; i++) 
    {   BOOST_CHECK (V1[ i - 1 ] <= V1[ i ]);
    };

    //-------------------- reverse sorted elements 4 threads ------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (NElem - i);

    block_indirect_sort ( V1.begin ( ), V1.end ( ), 4);
    for (unsigned i = 1; i < NElem; i++) 
    {   BOOST_CHECK (V1[ i - 1 ] <= V1[ i ]);
    };

    //-------------------- equal elements 4 threads ---------------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (1000);

    block_indirect_sort (V1.begin ( ), V1.end ( ), 4);
    for (unsigned i = 1; i < NElem; i++) 
    {   BOOST_CHECK (V1[ i - 1 ] == V1[ i ]);
    };
    
    //------------------ sorted elements  8 threads --------------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (i);    
    
     block_indirect_sort ( V1.begin ( ), V1.end ( ), 8);
    for (unsigned i = 1; i < NElem; i++) 
    {   BOOST_CHECK (V1[ i - 1 ] <= V1[ i ]);
    };

    //-------------------- reverse sorted elements 8 threads ------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (NElem - i);

    block_indirect_sort ( V1.begin ( ), V1.end ( ), 8);
    for (unsigned i = 1; i < NElem; i++) 
    {   BOOST_CHECK (V1[ i - 1 ] <= V1[ i ]);
    };

    //-------------------- equal elements 8 threads ---------------------------
    V1.clear ( );
    for (uint32_t i = 0; i < NElem; ++i) V1.push_back (1000);

    block_indirect_sort (V1.begin ( ), V1.end ( ), 8);
    for (unsigned i = 1; i < NElem; i++) 
    {   BOOST_CHECK (V1[ i - 1 ] == V1[ i ]);
    };
};

void test2 (void)
{
    typedef std::less<uint64_t> compare;
    std::vector< uint64_t > V1, V2;
    V1.reserve ( NELEM ) ;

    V2 = Vrandom;
    std::sort (V2.begin ( ), V2.end ( ));
    
    //------------------- random elements 0 threads ---------------------------
    V1 = Vrandom;
    block_indirect_sort (V1.begin ( ), V1.end ( ), compare(), 0);
    for (unsigned i = 0; i < V1.size(); i++)
    {   BOOST_CHECK (V1[i] == V2[i]);
    };

    //------------------- random elements 4 threads ---------------------------
    V1 = Vrandom;
    block_indirect_sort (V1.begin ( ), V1.end ( ), compare(), 4);
    for (unsigned i = 0; i < V1.size(); i++)
    {   BOOST_CHECK (V1[i] == V2[i]);
    };

    //------------------- random elements 8 threads ---------------------------
    V1 = Vrandom;
    block_indirect_sort (V1.begin ( ), V1.end ( ), compare(), 8);
    for (unsigned i = 0; i < V1.size(); i++)
    {   BOOST_CHECK (V1[i] == V2[i]);
    };

    //------------------- random elements 16 threads ---------------------------
    V1 = Vrandom;
    block_indirect_sort ( V1.begin ( ), V1.end ( ), compare(), 16);
    for (unsigned i = 0; i < V1.size(); i++)
    {   BOOST_CHECK (V1[i] == V2[i]);
    };

    //------------------- random elements 100 threads ---------------------------
    V1 = Vrandom;
    block_indirect_sort ( V1.begin ( ), V1.end ( ), compare(), 100);
    for (unsigned i = 1; i < V1.size(); i++)
    {   BOOST_CHECK (V1[i] == V2[i]);
    };
};

template<uint32_t NN>
struct int_array
{
    uint64_t M[NN];

    int_array(uint64_t number = 0)
    {   for (uint32_t i = 0; i < NN; ++i) M[i] = number;
    };

    bool operator<(const int_array<NN> &A) const
    {   return M[0] < A.M[0];
    };
};

template<class IA>
void test_int_array(uint32_t NELEM)
{
    std::vector<IA> V1;
    V1.reserve(NELEM);
    
    for (uint32_t i = 0; i < NELEM; ++i)
        V1.emplace_back(Vrandom[i]);

    bsp::block_indirect_sort(V1.begin(), V1.end());
    for (unsigned i = 1; i < NELEM; i++)
    {   BOOST_CHECK(not (V1[i] < V1[i-1]));
    };
};    
void test3 (void)
{
    test_int_array<int_array<1> >(1u << 20);
    test_int_array<int_array<2> >(1u << 19);
    test_int_array<int_array<8> >(1u << 17);
}

int test_main (int, char *[])
{   
    std::mt19937 my_rand (0);
    Vrandom.reserve (NELEM);
    for (uint32_t i = 0; i < NELEM; ++i) Vrandom.push_back (my_rand ( ));
    
    test1  ( );
    test2  ( );
    test3  ( );

    return 0;
};
