//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/vec_operations.hpp>
#   include <boost/qvm/vec.hpp>
#   include <boost/qvm/map_mat_vec.hpp>
#endif

#include "test_qvm_matrix.hpp"
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <int A,int B,bool LessThan=(A<B)>
    struct
    get_min
        {
        static int const value=B;
        };

    template <int A,int B>
    struct
    get_min<A,B,true>
        {
        static int const value=A;
        };

    template <int Rows,int Cols>
    void
    test()
        {
        using namespace boost::qvm;
        int const D=get_min<Rows,Cols>::value;
        test_qvm::matrix<M1,Rows,Cols> x(42,1);
        test_qvm::vector<V1,D> y=diag(x);
        for( int i=0; i!=D; ++i )
            y.b[i]=x.a[i][i];
        BOOST_QVM_TEST_EQ(y.a,y.b);
        diag(x) *= 2;
        for( int i=0; i!=D; ++i )
            x.b[i][i] *= 2;
        BOOST_QVM_TEST_EQ(x.a,x.b);
        diag(x) + diag(x);
        -diag(x);
        }
    }

int
main()
    {
    test<1,2>();
    test<1,3>();
    test<1,4>();
    test<1,5>();
    test<2,1>();
    test<3,1>();
    test<4,1>();
    test<5,1>();
    test<2,2>();
    test<3,3>();
    test<4,4>();
    test<5,5>();
    return boost::report_errors();
    }
