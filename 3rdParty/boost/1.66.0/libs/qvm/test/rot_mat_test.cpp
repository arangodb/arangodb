//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_operations.hpp>
#include "test_qvm_matrix.hpp"
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <int D>
    void
    test_x()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[0]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::matrix<M1,D,D> const m1=rot_mat<D>(axis,r);
            test_qvm::rotation_x(m1.b,r);
            BOOST_QVM_TEST_EQ(m1.a,m1.b);
            test_qvm::matrix<M1,D,D> m2(42,1);
            set_rot(m2,axis,r);
            test_qvm::rotation_x(m2.b,r);
            BOOST_QVM_TEST_EQ(m2.a,m2.b);
            test_qvm::matrix<M1,D,D> m3(42,1);
            test_qvm::matrix<M1,D,D> m4(42,1);
            rotate(m3,axis,r);
            m3 = m3*m1;
            BOOST_QVM_TEST_EQ(m3.a,m3.a);
            }
        }

    template <int D>
    void
    test_y()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[1]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::matrix<M1,D,D> m1=rot_mat<D>(axis,r);
            test_qvm::rotation_y(m1.b,r);
            BOOST_QVM_TEST_EQ(m1.a,m1.b);
            test_qvm::matrix<M1,D,D> m2(42,1);
            set_rot(m2,axis,r);
            test_qvm::rotation_y(m2.b,r);
            BOOST_QVM_TEST_EQ(m2.a,m2.b);
            test_qvm::matrix<M1,D,D> m3(42,1);
            test_qvm::matrix<M1,D,D> m4(42,1);
            rotate(m3,axis,r);
            m3 = m3*m1;
            BOOST_QVM_TEST_EQ(m3.a,m3.a);
            }
        }

    template <int D>
    void
    test_z()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[2]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::matrix<M1,D,D> m1=rot_mat<D>(axis,r);
            test_qvm::rotation_z(m1.b,r);
            BOOST_QVM_TEST_EQ(m1.a,m1.b);
            test_qvm::matrix<M1,D,D> m2(42,1);
            set_rot(m2,axis,r);
            test_qvm::rotation_z(m2.b,r);
            BOOST_QVM_TEST_EQ(m2.a,m2.b);
            test_qvm::matrix<M1,D,D> m3(42,1);
            test_qvm::matrix<M1,D,D> m4(42,1);
            rotate(m3,axis,r);
            m3 = m3*m1;
            BOOST_QVM_TEST_EQ(m3.a,m3.a);
            }
        }
    }

int
main()
    {
    test_x<3>();
    test_y<3>();
    test_z<3>();
    test_x<4>();
    test_y<4>();
    test_z<4>();
    test_x<5>();
    test_y<5>();
    test_z<5>();
    return boost::report_errors();
    }
