//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/mat.hpp>
#include "test_qvm_matrix.hpp"

namespace
    {
    template <class T,class U>
    struct same_type;

    template <class T>
    struct
    same_type<T,T>
        {
        };

    template <class T,class U>
    void
    check_deduction( T const &, U const & )
        {
        same_type<T,typename boost::qvm::deduce_mat<U>::type>();
        }

    template <int Rows,int Cols>
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::matrix<M1,Rows,Cols> m1=zero_mat<float,Rows,Cols>();
        for( int i=0; i!=Rows; ++i )
            for( int j=0; j!=Cols; ++j )
                BOOST_TEST(!m1.a[i][j]);
        test_qvm::matrix<M2,Rows,Cols> m2(42,1);
        set_zero(m2);
        for( int i=0; i!=Rows; ++i )
            for( int j=0; j!=Cols; ++j )
                BOOST_TEST(!m2.a[i][j]);
        check_deduction(mat<float,Rows,Cols>(),zero_mat<float,Rows,Cols>());
        check_deduction(mat<int,Rows,Cols>(),zero_mat<int,Rows,Cols>());
        }

    template <int Dim>
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::matrix<M1,Dim,Dim> m1=zero_mat<float,Dim>();
        for( int i=0; i!=Dim; ++i )
            for( int j=0; j!=Dim; ++j )
                BOOST_TEST(!m1.a[i][j]);
        }
    }

int
main()
    {
    test<1,2>();
    test<2,1>();
    test<2,2>();
    test<1,3>();
    test<3,1>();
    test<3,3>();
    test<1,4>();
    test<4,1>();
    test<4,4>();
    test<1,5>();
    test<5,1>();
    test<5,5>();
    test<2>();
    test<3>();
    test<4>();
    test<5>();
    return boost::report_errors();
    }
