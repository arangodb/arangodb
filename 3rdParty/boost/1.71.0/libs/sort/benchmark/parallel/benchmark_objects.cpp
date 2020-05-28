//----------------------------------------------------------------------------
/// @file benchmark_objects.cpp
/// @brief Benchmark of several sort methods with different objects
///
/// @author Copyright (c) 2016 Francisco José Tapia (fjtapia@gmail.com )\n
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
#include "boost/sort/common/int_array.hpp"

#include <boost/sort/sort.hpp>

#define NELEM 100000000
#define NMAXSTRING 10000000

using namespace std;
namespace bsc = boost::sort::common;
namespace bsp = boost::sort;

using bsc::time_point;
using bsc::now;
using bsc::subtract_time;
using bsc::fill_vector_uint64;
using bsc::write_file_uint64;
using bsc::int_array;
using bsc::H_comp;
using bsc::L_comp;



template <class IA>
void Generator_random(uint64_t N);

template <class IA>
void Generator_sorted(uint64_t N);
template <class IA>
void Generator_sorted_end(uint64_t N, size_t n_last );

template <class IA>
void Generator_sorted_middle(uint64_t N, size_t n_last );

template <class IA>
void Generator_reverse_sorted(uint64_t N);

template <class IA>
void Generator_reverse_sorted_end(uint64_t N, size_t n_last );

template <class IA>
void Generator_reverse_sorted_middle(uint64_t N, size_t n_last );

template < class IA >
struct H_rightshift {
  inline uint64_t operator()(const IA& A1, unsigned offset) {
    return A1.counter() >> offset;
  }
};

template < class IA >
struct L_rightshift {
  inline uint64_t operator()(const IA& A1, unsigned offset) {
    return A1.M[0] >> offset;
  }
};

template <class IA, class rightshift, class compare>
int Test(std::vector<IA> &B, rightshift shift, compare comp, std::vector<double> & V );

template <class IA>
void Test_size ( uint64_t N);

void Print_vectors ( std::vector<double> & V1, std::vector<double> & V2);

int main(int argc, char *argv[])
{
    cout << "\n\n";
    cout << "************************************************************\n";
    cout << "**                                                        **\n";
    cout << "**        B O O S T    S O R T    P A R A L L E L         **\n";
    cout << "**                                                        **\n";
    cout << "**          O B J E C T S     B E N C H M A R K           **\n";
    cout << "**                                                        **\n";
    cout << "************************************************************\n";
    cout << std::endl;

    cout << "=============================================================\n";
    cout << "=            OBJECT COMPARISON                              =\n";
    cout << "=          ---------------------                            =\n";
    cout << "=                                                           =\n";
    cout << "= The objects are arrays of 64 bits numbers                 =\n";
    cout << "= They are compared in two ways :                           =\n";
    cout << "= (H) Heavy : The comparison is the sum of all the numbers  =\n";
    cout << "=             of the array                                  =\n";
    cout << "= (L) Light : The comparison is with the first element of   =\n";
    cout << "=             the array, as a key                           =\n";
    cout << "=                                                           =\n";
    cout << "============================================================\n";
    cout << "\n\n";

    //-----------------------------------------------------------------------
    //                  I N T _ A R R A Y < 1 >
    //-----------------------------------------------------------------------
    cout << "************************************************************\n";
    cout << "**                                                        **\n";
    cout << "              "<<NELEM<<" OBJECTS UINT64_T [1]\n";
    cout << "**                                                        **\n";
    cout << "************************************************************\n";

    Test_size<int_array<1> >(NELEM);

    //-----------------------------------------------------------------------
    //                  I N T _ A R R A Y < 4 >
    //-----------------------------------------------------------------------
    cout << "************************************************************\n";
    cout << "**                                                        **\n";
    cout << "              "<<(NELEM >>2)<<" OBJECTS UINT64_T [4]\n";
    cout << "**                                                        **\n";
    cout << "************************************************************\n";
    Test_size<int_array<4> >(NELEM >> 2);

    //-----------------------------------------------------------------------
    //                  I N T _ A R R A Y < 8 >
    //-----------------------------------------------------------------------
    cout << "************************************************************\n";
    cout << "**                                                        **\n";
    cout << "            "<<(NELEM >>3)<<" OBJECTS UINT64_T [8]\n";
    cout << "**                                                        **\n";
    cout << "************************************************************\n";
    Test_size<int_array<8> >(NELEM >> 3);

    //-----------------------------------------------------------------------
    //                  I N T _ A R R A Y < 1 6 >
    //-----------------------------------------------------------------------
    cout << "************************************************************\n";
    cout << "**                                                        **\n";
    cout << "            "<<(NELEM >>4)<<" OBJECTS UINT64_T [16]\n";
    cout << "**                                                        **\n";
    cout << "************************************************************\n";
    Test_size<int_array<16> >(NELEM >> 4);

    //-----------------------------------------------------------------------
    //                  I N T _ A R R A Y < 6 4 >
    //-----------------------------------------------------------------------
    cout << "************************************************************\n";
    cout << "**                                                        **\n";
    cout << "            "<< (NELEM >>6)<<" OBJECTS UINT64_T [64]\n";
    cout << "**                                                        **\n";
    cout << "************************************************************\n";
    Test_size<int_array<64> >(NELEM >> 6);

   return 0;
}

template <class IA>
void Test_size ( uint64_t N)
{
    cout<<"[ 1 ] block_indirect_sort      [ 2 ] sample_sort\n";
    cout<<"[ 3 ] parallel_stable_sort\n\n";

    cout << "                    |   [ 1 ]   |   [ 2 ]   |   [ 3 ]   |\n";
    cout << "                    |  H     L  |  H     L  |  H     L  |\n";
    cout << "--------------------+-----------+-----------+-----------+\n";
    std::string empty_line = "                    |           |           |           |\n";

    cout<<"random              |";
    Generator_random<IA>(N);

    cout<<empty_line;
    cout<<"sorted              |";
    Generator_sorted<IA>(N);

    cout<<"sorted + 0.1% end   |";
    Generator_sorted_end<IA>(N, N / 1000);

    cout<<"sorted +   1% end   |";
    Generator_sorted_end<IA>(N, N / 100);

    cout<<"sorted +  10% end   |";
    Generator_sorted_end<IA>(N, N / 10);

    cout<<empty_line;
    cout<<"sorted + 0.1% mid   |";
    Generator_sorted_middle<IA>(N, N / 1000);

    cout<<"sorted +   1% mid   |";
    Generator_sorted_middle<IA>(N, N / 100);

    cout<<"sorted +  10% mid   |";
    Generator_sorted_middle<IA>(N, N / 10);

    cout<<empty_line;
    cout<<"reverse sorted      |";
    Generator_reverse_sorted<IA>(N);

    cout<<"rv sorted + 0.1% end|";
    Generator_reverse_sorted_end<IA>(N, N / 1000);

    cout<<"rv sorted +   1% end|";
    Generator_reverse_sorted_end<IA>(N, N / 100);

    cout<<"rv sorted +  10% end|";
    Generator_reverse_sorted_end<IA>(N, N / 10);

    cout<<empty_line;
    cout<<"rv sorted + 0.1% mid|";
    Generator_reverse_sorted_middle<IA>(N, N / 1000);

    cout<<"rv sorted +   1% mid|";
    Generator_reverse_sorted_middle<IA>(N, N / 100);

    cout<<"rv sorted +  10% mid|";
    Generator_reverse_sorted_middle<IA>(N, N / 10);
    cout<< "--------------------+-----------+-----------+-----------+\n";
    cout<<endl<<endl<<endl;
}
void Print_vectors ( std::vector<double> & V1, std::vector<double> & V2)
{
    assert ( V1.size() == V2.size());
    std::cout<<std::setprecision(2)<<std::fixed;
    for ( uint32_t i =0 ; i < V1.size() ; ++i)
    {   std::cout<<std::right<<std::setw(5)<<V1[i]<<" ";
        std::cout<<std::right<<std::setw(5)<<V2[i]<<"|";
    };
    std::cout<<std::endl;
}

template <class IA>
void Generator_random(uint64_t N)
{
    bsc::uint64_file_generator gen("input.bin");
    vector<IA> A;
    A.reserve(N);
    std::vector<double> V1, V2 ;

    gen.reset();
    A.clear();
    for (uint32_t i = 0; i < N; i++) A.emplace_back(IA::generate(gen));

    Test(A, H_rightshift<IA>(), H_comp<IA>(), V1);

    Test(A, L_rightshift<IA>(), L_comp<IA>(), V2);
    Print_vectors ( V1, V2 ) ;
};
template <class IA>
void Generator_sorted(uint64_t N)
{
    bsc::uint64_file_generator gen("input.bin");
    vector<IA> A;
    A.reserve(N);
    std::vector<double> V1, V2 ;

    gen.reset();
    A.clear();
    for (uint32_t i = 0; i < N; i++) A.emplace_back(IA::generate(gen));

    std::sort( A.begin() , A.end(),H_comp<IA>() );
    Test(A, H_rightshift<IA>(), H_comp<IA>(), V1);

    std::sort( A.begin() , A.end(),L_comp<IA>() );
    Test(A, L_rightshift<IA>(), L_comp<IA>(), V2);
    Print_vectors ( V1, V2 ) ;
};

template <class IA>
void Generator_sorted_end(uint64_t N, size_t n_last )
{
    bsc::uint64_file_generator gen("input.bin");
    vector<IA> A;
    A.reserve(N);
    std::vector<double> V1, V2 ;

    gen.reset();
    A.clear();
    for (uint32_t i = 0; i < (N + n_last); i++)
        A.emplace_back(IA::generate(gen));
    std::sort ( A.begin() , A.begin() + N ,H_comp<IA>());

    Test(A, H_rightshift<IA>(), H_comp<IA>(),V1);
    std::sort ( A.begin() , A.begin() + N ,L_comp<IA>());

    Test(A, L_rightshift<IA>(), L_comp<IA>(), V2);
    Print_vectors ( V1, V2 ) ;
};
template <class IA>
void Generator_sorted_middle(uint64_t N, size_t n_last )
{
    bsc::uint64_file_generator gen("input.bin");
    std::vector<double> V1, V2 ;
    vector<IA> A;
    A.reserve(N + n_last);

    {   A.clear() ;
        gen.reset();
        vector<IA> B, C;
        C.reserve(N + n_last);
        B.reserve ( n_last);
        for (uint32_t i = 0; i < (N + n_last); i++)
            C.emplace_back(IA::generate(gen));

        for ( size_t i = N ; i < C.size() ; ++i)
            B.push_back ( std::move ( C[i]));

        C.resize ( N);

        std::sort ( C.begin() , C.end() ,H_comp<IA>());
        size_t step = N /n_last  ;
        size_t pos = 0 ;

        for ( size_t i =0 ; i < B.size() ; ++i)
        {   A.push_back ( B[i]);
            for ( size_t k = 0 ; k < step ; ++k )
                A.push_back ( C[pos++] );
        };
        while ( pos < C.size() )
            A.push_back ( C[pos++]);
    };

    Test(A, H_rightshift<IA>(), H_comp<IA>(), V1);

    {   A.clear() ;
        gen.reset();
        vector<IA> B,C;
        C.reserve(N + n_last);
        B.reserve ( n_last);
        for (uint32_t i = 0; i < (N + n_last); i++)
            C.emplace_back(IA::generate(gen));

        for ( size_t i = N ; i < C.size() ; ++i)
            B.push_back ( std::move ( C[i]));

        C.resize ( N);

        std::sort ( C.begin() , C.end() ,L_comp<IA>());
        size_t step = N /n_last  ;
        size_t pos = 0 ;

        for ( size_t i =0 ; i < B.size() ; ++i)
        {   A.push_back ( B[i]);
            for ( size_t k = 0 ; k < step ; ++k )
                A.push_back ( C[pos++] );
        };
        while ( pos < C.size() )
            A.push_back ( C[pos++]);
    };
    Test(A, L_rightshift<IA>(), L_comp<IA>(), V2);
    Print_vectors ( V1, V2 ) ;
};
template <class IA>
void Generator_reverse_sorted(uint64_t N)
{
    bsc::uint64_file_generator gen("input.bin");
    vector<IA> A;
    A.reserve(N);
    std::vector<double> V1, V2 ;

    gen.reset();
    A.clear();
    for (uint32_t i = 0; i < N; i++) A.emplace_back(IA::generate(gen));

    std::sort( A.begin() , A.end(),H_comp<IA>() );
    for ( size_t i = 0; i < (A.size() >>1) ; ++i)
        std::swap ( A[i], A[A.size() - i -1 ]);

    Test(A, H_rightshift<IA>(), H_comp<IA>(), V1);

    std::sort( A.begin() , A.end(),L_comp<IA>() );
    for ( size_t i = 0; i < (A.size() >>1) ; ++i)
        std::swap ( A[i], A[A.size() - i -1 ]);
    Test(A, L_rightshift<IA>(), L_comp<IA>(), V2);
    Print_vectors ( V1, V2 ) ;
};
template <class IA>
void Generator_reverse_sorted_end(uint64_t N, size_t n_last )
{
    bsc::uint64_file_generator gen("input.bin");
    vector<IA> A;
    A.reserve(N);
    std::vector<double> V1, V2 ;

    gen.reset();
    A.clear();
    for (uint32_t i = 0; i < (N + n_last); i++)
        A.emplace_back(IA::generate(gen));
    std::sort ( A.begin() , A.begin() + N ,H_comp<IA>());
    for ( size_t i =0 ; i < (N>>1); ++i)
        std::swap ( A[i], A[N-1-i]);

    Test(A, H_rightshift<IA>(), H_comp<IA>(), V1);
    std::sort ( A.begin() , A.begin() + N ,L_comp<IA>());
    for ( size_t i =0 ; i < (N>>1); ++i)
        std::swap ( A[i], A[N-1-i]);

    Test(A, L_rightshift<IA>(), L_comp<IA>(), V2);
    Print_vectors ( V1, V2 ) ;
};
template <class IA>
void Generator_reverse_sorted_middle(uint64_t N, size_t n_last )
{
    bsc::uint64_file_generator gen("input.bin");
    std::vector<double> V1, V2 ;
    vector<IA> A;
    A.reserve(N + n_last);

    {   A.clear() ;
        gen.reset();
        vector<IA> B,C;
        C.reserve(N + n_last);
        B.reserve ( n_last);
        for (uint32_t i = 0; i < (N + n_last); i++)
            C.emplace_back(IA::generate(gen));

        for ( size_t i = N ; i < C.size() ; ++i)
            B.push_back ( std::move ( C[i]));

        C.resize ( N);

        std::sort ( C.begin() , C.end() ,H_comp<IA>());

        for ( size_t i =0 ; i < (N>>1); ++i)
            std::swap ( C[i], C[N-1-i]);

        size_t step = N /n_last  ;
        size_t pos = 0 ;

        for ( size_t i =0 ; i < B.size() ; ++i)
        {   A.push_back ( B[i]);
            for ( size_t k = 0 ; k < step ; ++k )
                A.push_back ( C[pos++ ] );
        };
        while ( pos < C.size() )
            A.push_back ( C[pos++]);
    };

    Test(A, H_rightshift<IA>(), H_comp<IA>(), V1);

    {   A.clear() ;
        gen.reset();
        vector<IA> B,C;
        C.reserve(N + n_last);
        B.reserve ( n_last);
        for (uint32_t i = 0; i < (N + n_last); i++)
            C.emplace_back(IA::generate(gen));

        for ( size_t i = N ; i < C.size() ; ++i)
            B.push_back ( std::move ( C[i]));

        C.resize ( N);

        std::sort ( C.begin() , C.end() ,L_comp<IA>());

        for ( size_t i =0 ; i < (N>>1); ++i)
            std::swap ( C[i], C[N-1-i]);
        size_t step = N /n_last  ;
        size_t pos = 0 ;

        for ( size_t i =0 ; i < B.size() ; ++i)
        {   A.push_back ( B[i]);
            for ( size_t k = 0 ; k < step ; ++k )
                A.push_back ( C[pos++ ] );
        };
        while ( pos < C.size() )
            A.push_back ( C[pos++]);
    };

    Test(A, L_rightshift<IA>(), L_comp<IA>(), V2);
    Print_vectors ( V1, V2 ) ;
};

template<class IA, class rightshift, class compare>
int Test (std::vector<IA> &B, rightshift shift, compare comp,std::vector<double> &V)
{   //---------------------------- begin --------------------------------
    double duration;
    time_point start, finish;
    std::vector<IA> A (B);
    V.clear() ;

    //--------------------------------------------------------------------
    A = B;
    start = now ();
    bsp::block_indirect_sort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);
    std::vector<IA> sorted(A);

    A = B;
    start = now ();
    bsp::sample_sort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);

    A = B;
    start = now ();
    bsp::parallel_stable_sort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);
    //cout << duration << " secs\n";
/*
    A = B;
    start = now ();
    spinsort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);

    A = B;
    start = now ();
    timsort (A.begin (), A.end (), comp);
    finish = now ();
    duration = subtract_time (finish, start);
    V.push_back (duration);


    A = B;
    //cout << "Boost spreadsort             : ";
    int sorted_count = 0;
    for (unsigned i = 0; i + 1 < A.size(); ++i)
    {
        if (comp(A[i], A[i+1]))
        {
            sorted_count++;
        };
    };
    start = now ();
    boost::sort::spreadsort::integer_sort (A.begin (), A.end (), shift, comp);
    finish = now ();
    duration = subtract_time (finish, start);
    int identical = 0;
    for (unsigned i = 0; i + 1 < A.size(); ++i)
    {
        if (A[i].counter() != sorted[i].counter())
        {
            cout << "incorrect!" << std::endl;
        }
        if (!comp(A[i], A[i+1]) && !comp(A[i+1], A[i]))
        {
            identical++;
        };
    };
    //    cout << "identical: " << identical << " pre-sorted: " << sorted_count << std::endl;
    V.push_back (duration);
    //cout << duration << " secs\n";
*/
    return 0;
};
