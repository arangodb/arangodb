//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/mat.hpp>
#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <class T,class U> struct same_type_tester;
    template <class T> struct same_type_tester<T,T> { };
    template <class T,class U> void test_same_type( T, U ) { same_type_tester<T,U>(); }

    template <int Rows,int Cols>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,Rows,Cols> const x(42,2);
            {
            test_qvm::matrix<M1,Rows,Cols> const y(42,1);
            test_same_type(x,x+y);
            test_qvm::matrix<M1,Rows,Cols> r=x+y;
            test_qvm::add_m(r.b,x.b,y.b);
            BOOST_QVM_TEST_EQ(r.a,r.b);
            }
            {
            test_qvm::matrix<M1,Rows,Cols> const y(42,1);
            test_qvm::matrix<M2,Rows,Cols> r=mref(x)+y;
            test_qvm::add_m(r.b,x.b,y.b);
            BOOST_QVM_TEST_EQ(r.a,r.b);
            }
            {
            test_qvm::matrix<M1,Rows,Cols> const y(42,1);
            test_qvm::matrix<M2,Rows,Cols> r=x+mref(y);
            test_qvm::add_m(r.b,x.b,y.b);
            BOOST_QVM_TEST_EQ(r.a,r.b);
            }
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
    return boost::report_errors();
    }
