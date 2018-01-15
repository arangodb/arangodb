//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/operations.hpp>
#include <boost/qvm/map_mat_mat.hpp>
#include "test_qvm_matrix.hpp"
#include "test_qvm_quaternion.hpp"
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <int Rows,int Cols>
    void
    test_matrix()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,Rows,Cols> const x(42,1);
        test_qvm::matrix<M2,Rows,Cols> const y=convert_to< test_qvm::matrix<M2,Rows,Cols> >(x);
        BOOST_QVM_TEST_EQ(x.a,y.a);
        }

    template <int Dim>
    void
    test_vector()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::vector<V1,Dim> const x(42,1);
        test_qvm::vector<V2,Dim> const y=convert_to< test_qvm::vector<V2,Dim> >(x);
        BOOST_QVM_TEST_EQ(x.a,y.a);
        }

    void
    test_quaternion()
        {
        using namespace boost::qvm;
        test_qvm::quaternion<Q1> x(42,1);
        normalize(x);
            {
            test_qvm::quaternion<Q2> const y=convert_to< test_qvm::quaternion<Q2> >(x);
            BOOST_QVM_TEST_EQ(x.a,y.a);
            }
            {
            test_qvm::matrix<M1,3,3> const my=convert_to< test_qvm::matrix<M1,3,3> >(x);
            test_qvm::quaternion<Q1> const qy=convert_to< test_qvm::quaternion<Q1> >(my);
            BOOST_QVM_TEST_CLOSE(x.a,qy.a,0.00001f);
            }
            {
            test_qvm::matrix<M1,4,4> const my=convert_to< test_qvm::matrix<M1,4,4> >(x);
            BOOST_TEST(my.a[0][3]==0);
            BOOST_TEST(my.a[1][3]==0);
            BOOST_TEST(my.a[2][3]==0);
            BOOST_TEST(my.a[3][0]==0);
            BOOST_TEST(my.a[3][1]==0);
            BOOST_TEST(my.a[3][2]==0);
            BOOST_TEST(my.a[3][3]==1);
            test_qvm::quaternion<Q1> const qy=convert_to< test_qvm::quaternion<Q1> >(del_row_col<3,3>(my));
            BOOST_QVM_TEST_CLOSE(x.a,qy.a,0.00001f);
            }
        }
    }

int
main()
    {
    test_matrix<1,2>();
    test_matrix<2,1>();
    test_matrix<2,2>();
    test_matrix<1,3>();
    test_matrix<3,1>();
    test_matrix<3,3>();
    test_matrix<1,4>();
    test_matrix<4,1>();
    test_matrix<4,4>();
    test_matrix<1,5>();
    test_matrix<5,1>();
    test_matrix<5,5>();
    test_quaternion();
    test_vector<2>();
    test_vector<3>();
    test_vector<4>();
    test_vector<5>();
    return boost::report_errors();
    }
