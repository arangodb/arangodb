//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/map_mat_mat.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/mat_traits_array.hpp>
#include <boost/qvm/mat.hpp>
#include "test_qvm.hpp"
#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <int Rows,int Cols,int C1,int C2>
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::matrix<M1,Rows,Cols> x(42,1);
        float r1[Rows][Cols];
        for( int i=0; i!=Rows; ++i )
            for( int j=0; j!=Cols; ++j )
                r1[i][j]=x.a[i][j==C1?C2:j==C2?C1:j];
        float r2[Rows][Cols];
        assign(r2,swap_cols<C1,C2>(x));
        BOOST_QVM_TEST_EQ(r1,r2);
        swap_cols<C1,C2>(x) *= 2;
        for( int i=0; i!=Rows; ++i )
            for( int j=0; j!=Cols; ++j )
                r1[i][j]=x.a[i][j==C1?C2:j==C2?C1:j];
        assign(r2,swap_cols<C1,C2>(x));
        BOOST_QVM_TEST_EQ(r1,r2);
        swap_cols<C1,C2>(x)+swap_cols<C1,C2>(x);
        -swap_cols<C1,C2>(x);
        }
    }

int
main()
    {
    test<2,2,0,0>();
    test<2,2,0,1>();
    test<2,2,1,0>();
    test<2,2,1,1>();

    test<3,3,0,0>();
    test<3,3,0,1>();
    test<3,3,0,2>();
    test<3,3,1,0>();
    test<3,3,1,1>();
    test<3,3,1,2>();
    test<3,3,2,0>();
    test<3,3,2,1>();
    test<3,3,2,2>();

    test<4,4,0,0>();
    test<4,4,0,1>();
    test<4,4,0,2>();
    test<4,4,0,3>();
    test<4,4,1,0>();
    test<4,4,1,1>();
    test<4,4,1,2>();
    test<4,4,1,3>();
    test<4,4,2,0>();
    test<4,4,2,1>();
    test<4,4,2,2>();
    test<4,4,2,3>();
    test<4,4,3,0>();
    test<4,4,3,1>();
    test<4,4,3,2>();
    test<4,4,3,3>();

    test<5,5,0,0>();
    test<5,5,0,1>();
    test<5,5,0,2>();
    test<5,5,0,3>();
    test<5,5,0,4>();
    test<5,5,1,0>();
    test<5,5,1,1>();
    test<5,5,1,2>();
    test<5,5,1,3>();
    test<5,5,1,4>();
    test<5,5,2,0>();
    test<5,5,2,1>();
    test<5,5,2,2>();
    test<5,5,2,3>();
    test<5,5,2,4>();
    test<5,5,3,0>();
    test<5,5,3,1>();
    test<5,5,3,2>();
    test<5,5,3,3>();
    test<5,5,3,4>();
    test<5,5,4,0>();
    test<5,5,4,1>();
    test<5,5,4,2>();
    test<5,5,4,3>();
    test<5,5,4,4>();
    return boost::report_errors();
    }
