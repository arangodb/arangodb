//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/map_mat_vec.hpp>
#include <boost/qvm/vec_traits_array.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/vec.hpp>
#include "test_qvm.hpp"
#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <int Rows,int Cols,int Col>
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::matrix<M1,Rows,Cols> x(42,1);
        float r1[Rows];
        for( int i=0; i!=Rows; ++i )
            r1[i]=x.a[i][Col];
        float r2[Rows];
        assign(r2,col<Col>(x));
        BOOST_QVM_TEST_EQ(r1,r2);
        col<Col>(x) *= 2;
        for( int i=0; i!=Rows; ++i )
            r1[i]=x.a[i][Col];
        assign(r2,col<Col>(x));
        BOOST_QVM_TEST_EQ(r1,r2);
        col<Col>(x) + col<Col>(x);
        -col<Col>(x);
        }
    }

int
main()
    {
    test<1,2,0>();
    test<1,2,1>();
    test<1,3,0>();
    test<1,3,1>();
    test<1,3,2>();
    test<1,4,0>();
    test<1,4,1>();
    test<1,4,2>();
    test<1,4,3>();
    test<1,5,0>();
    test<1,5,1>();
    test<1,5,2>();
    test<1,5,3>();
    test<1,5,4>();

    test<2,2,0>();
    test<2,2,1>();
    test<2,3,0>();
    test<2,3,1>();
    test<2,3,2>();
    test<2,4,0>();
    test<2,4,1>();
    test<2,4,2>();
    test<2,4,3>();
    test<2,5,0>();
    test<2,5,1>();
    test<2,5,2>();
    test<2,5,3>();
    test<2,5,4>();

    test<3,2,0>();
    test<3,2,1>();
    test<3,3,0>();
    test<3,3,1>();
    test<3,3,2>();
    test<3,4,0>();
    test<3,4,1>();
    test<3,4,2>();
    test<3,4,3>();
    test<3,5,0>();
    test<3,5,1>();
    test<3,5,2>();
    test<3,5,3>();
    test<3,5,4>();

    test<4,2,0>();
    test<4,2,1>();
    test<4,3,0>();
    test<4,3,1>();
    test<4,3,2>();
    test<4,4,0>();
    test<4,4,1>();
    test<4,4,2>();
    test<4,4,3>();
    test<4,5,0>();
    test<4,5,1>();
    test<4,5,2>();
    test<4,5,3>();
    test<4,5,4>();

    test<5,2,0>();
    test<5,2,1>();
    test<5,3,0>();
    test<5,3,1>();
    test<5,3,2>();
    test<5,4,0>();
    test<5,4,1>();
    test<5,4,2>();
    test<5,4,3>();
    test<5,5,0>();
    test<5,5,1>();
    test<5,5,2>();
    test<5,5,3>();
    test<5,5,4>();

    return boost::report_errors();
    }
