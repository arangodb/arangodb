//----------------------------------------------------------------------------
/// @file benchmark_numbers.cpp
/// @brief Benchmark of several sort methods with integer objects
///
/// @author Copyright (c) 2017 Francisco José Tapia (fjtapia@gmail.com )\n
///         Distributed under the Boost Software License, Version 1.0.\n
///         ( See accompanying file LICENSE_1_0.txt or copy at
///           http://www.boost.org/LICENSE_1_0.txt )
///
///         This program use for comparison purposes, the Threading Building
///         Blocks which license is the GNU General Public License, version 2
///         as  published  by  the  Free Software Foundation.
///
/// @version 0.1
///
/// @remarks
//-----------------------------------------------------------------------------
#include <algorithm>

#include <iostream>
#include <iomanip>
#include <random>
#include <stdlib.h>
#include <vector>

#include <boost/sort/common/time_measure.hpp>
#include <boost/sort/common/file_vector.hpp>
#include <boost/sort/common/int_array.hpp>

#include <boost/sort/sort.hpp>

#define NELEM 100000000

using namespace std;
namespace bsort = boost::sort;
namespace bsc = boost::sort::common;

using bsc::time_point;
using bsc::now;
using bsc::subtract_time;
using bsc::fill_vector_uint64;
using bsc::write_file_uint64;


void Generator_random (void);
void Generator_sorted (void);
void Generator_sorted_end (size_t n_last);
void Generator_sorted_middle (size_t n_last);
void Generator_reverse_sorted (void);
void Generator_reverse_sorted_end (size_t n_last);
void Generator_reverse_sorted_middle (size_t n_last);

template<class IA, class compare>
int Test (std::vector<IA> &B, compare comp = compare ());

int main (int argc, char *argv[])
{
    cout << "\n\n";
    cout << "************************************************************\n";
    cout << "**                                                        **\n";
    cout << "**        B O O S T    S O R T   P A R A L L E L          **\n";
    cout << "**          I N T E G E R    B E N C H M A R K            **\n";
    cout << "**                                                        **\n";
    cout << "**        SORT OF 100 000 000 NUMBERS OF 64 BITS          **\n";
    cout << "**                                                        **\n";
    cout << "************************************************************\n";
    cout << std::endl;

    cout<<"[ 1 ] block_indirect_sort      [ 2 ] sample_sort\n";
    cout<<"[ 3 ] parallel_stable_sort\n\n";
    cout<<"                    |      |      |      |\n";
    cout<<"                    | [ 1 ]| [ 2 ]| [ 3 ]|\n";
    cout<<"--------------------+------+------+------+\n";
    std::string empty_line =
           "                    |      |      |      |\n";
    cout<<"random              |";
    Generator_random ();
    cout<<empty_line;
    cout<<"sorted              |";
    Generator_sorted ();

    cout<<"sorted + 0.1% end   |";
    Generator_sorted_end (NELEM / 1000);

    cout<<"sorted +   1% end   |";
    Generator_sorted_end (NELEM / 100);

    cout<<"sorted +  10% end   |";
    Generator_sorted_end (NELEM / 10);

    cout<<empty_line;
    cout<<"sorted + 0.1% mid   |";
    Generator_sorted_middle (NELEM / 1000);

    cout<<"sorted +   1% mid   |";
    Generator_sorted_middle (NELEM / 100);

    cout<<"sorted +  10% mid   |";
    Generator_sorted_middle (NELEM / 10);

    cout<<empty_line;
    cout<<"reverse sorted      |";
    Generator_reverse_sorted ();

    cout<<"rv sorted + 0.1% end|";
    Generator_reverse_sorted_end (NELEM / 1000);

    cout<<"rv sorted +   1% end|";
    Generator_reverse_sorted_end (NELEM / 100);

    cout<<"rv sorted +  10% end|";
    Generator_reverse_sorted_end (NELEM / 10);

    cout<<empty_line;
    cout<<"rv sorted + 0.1% mid|";
    Generator_reverse_sorted_middle (NELEM / 1000);

    cout<<"rv sorted +   1% mid|";
    Generator_reverse_sorted_middle (NELEM / 100);

    cout<<"rv sorted +  10% mid|";
    Generator_reverse_sorted_middle (NELEM / 10);
    cout<<"--------------------+------+------+------+\n";
    cout<<endl<<endl ;
    return 0;
}
void
Generator_random (void)
{
    vector<uint64_t> A;
    A.reserve (NELEM);
    A.clear ();
    if (fill_vector_uint64 ("input.bin", A, NELEM) != 0)
    {
        std::cout << "Error in the input file\n";
        return;
    };
    Test<uint64_t, std::less<uint64_t>> (A);
}
;
void
Generator_sorted (void)
{
    vector<uint64_t> A;

    A.reserve (NELEM);
    A.clear ();
    for (size_t i = 0; i < NELEM; ++i)
        A.push_back (i);
    Test<uint64_t, std::less<uint64_t>> (A);

}

void Generator_sorted_end (size_t n_last)
{
    vector<uint64_t> A;
    A.reserve (NELEM);
    A.clear ();
    if (fill_vector_uint64 ("input.bin", A, NELEM + n_last) != 0)
    {
        std::cout << "Error in the input file\n";
        return;
    };
    std::sort (A.begin (), A.begin () + NELEM);

    Test<uint64_t, std::less<uint64_t>> (A);

}
;
void Generator_sorted_middle (size_t n_last)
{
    vector<uint64_t> A, B, C;
    A.reserve (NELEM);
    A.clear ();
    if (fill_vector_uint64 ("input.bin", A, NELEM + n_last) != 0)
    {
        std::cout << "Error in the input file\n";
        return;
    };
    for (size_t i = NELEM; i < A.size (); ++i)
        B.push_back (std::move (A[i]));
    A.resize ( NELEM);
    for (size_t i = 0; i < (NELEM >> 1); ++i)
        std::swap (A[i], A[NELEM - 1 - i]);

    std::sort (A.begin (), A.end ());
    size_t step = NELEM / n_last + 1;
    size_t pos = 0;

    for (size_t i = 0; i < B.size (); ++i, pos += step)
    {
        C.push_back (B[i]);
        for (size_t k = 0; k < step; ++k)
            C.push_back (A[pos + k]);
    };
    while (pos < A.size ())
        C.push_back (A[pos++]);
    A = C;
    Test<uint64_t, std::less<uint64_t>> (A);
}
;
void Generator_reverse_sorted (void)
{
    vector<uint64_t> A;

    A.reserve (NELEM);
    A.clear ();
    for (size_t i = NELEM; i > 0; --i)
        A.push_back (i);
    Test<uint64_t, std::less<uint64_t>> (A);
}
void Generator_reverse_sorted_end (size_t n_last)
{
    vector<uint64_t> A;
    A.reserve (NELEM);
    A.clear ();
    if (fill_vector_uint64 ("input.bin", A, NELEM + n_last) != 0)
    {
        std::cout << "Error in the input file\n";
        return;
    };
    std::sort (A.begin (), A.begin () + NELEM);
    for (size_t i = 0; i < (NELEM >> 1); ++i)
        std::swap (A[i], A[NELEM - 1 - i]);

    Test<uint64_t, std::less<uint64_t>> (A);
}
void Generator_reverse_sorted_middle (size_t n_last)
{
    vector<uint64_t> A, B, C;
    A.reserve (NELEM);
    A.clear ();
    if (fill_vector_uint64 ("input.bin", A, NELEM + n_last) != 0)
    {
        std::cout << "Error in the input file\n";
        return;
    };
    for (size_t i = NELEM; i < A.size (); ++i)
        B.push_back (std::move (A[i]));
    A.resize ( NELEM);
    for (size_t i = 0; i < (NELEM >> 1); ++i)
        std::swap (A[i], A[NELEM - 1 - i]);

    std::sort (A.begin (), A.end ());
    size_t step = NELEM / n_last + 1;
    size_t pos = 0;

    for (size_t i = 0; i < B.size (); ++i, pos += step)
    {
        C.push_back (B[i]);
        for (size_t k = 0; k < step; ++k)
            C.push_back (A[pos + k]);
    };
    while (pos < A.size ())
        C.push_back (A[pos++]);
    A = C;
    Test<uint64_t, std::less<uint64_t>> (A);
};


template<class IA, class compare>
int Test (std::vector<IA> &B,  compare comp)
{   //---------------------------- begin --------------------------------
    double duration;
    time_point start, finish;
    std::vector<IA> A (B);
    std::vector<double> V;

    //--------------------------------------------------------------------
    A = B;
    start = now ();
    bsort::block_indirect_sort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);

    A = B;
    start = now ();
    bsort::sample_sort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);

    A = B;
    start = now ();
    bsort::parallel_stable_sort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);

    //-----------------------------------------------------------------------
    // printing the vector
    //-----------------------------------------------------------------------
    std::cout<<std::setprecision(2)<<std::fixed;
    for ( uint32_t i =0 ; i < V.size() ; ++i)
    {   std::cout<<std::right<<std::setw(5)<<V[i]<<" |";
    };
    std::cout<<std::endl;
    return 0;
};

