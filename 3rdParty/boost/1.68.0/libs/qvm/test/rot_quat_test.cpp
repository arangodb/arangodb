//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_operations.hpp>
#include <boost/qvm/mat_operations.hpp>
#include "test_qvm_matrix.hpp"
#include "test_qvm_quaternion.hpp"
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    void
    test_x()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[0]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::quaternion<Q1> q1=rot_quat(axis,r);
            test_qvm::matrix<M1,3,3> x1=convert_to< test_qvm::matrix<M1,3,3> >(q1);
            test_qvm::rotation_x(x1.b,r);
            BOOST_QVM_TEST_CLOSE(x1.a,x1.b,0.000001f);
            test_qvm::quaternion<Q2> q2(42,1);
            set_rot(q2,axis,r);
            test_qvm::matrix<M2,3,3> x2=convert_to< test_qvm::matrix<M2,3,3> >(q2);
            test_qvm::rotation_x(x2.b,r);
            BOOST_QVM_TEST_CLOSE(x2.a,x2.b,0.000001f);
            test_qvm::quaternion<Q1> q3(42,1);
            test_qvm::quaternion<Q1> q4(42,1);
            rotate(q3,axis,r);
            q3 = q3*q1;
            BOOST_QVM_TEST_EQ(q3.a,q3.a);
            }
        }

    void
    test_y()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[1]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::quaternion<Q1> q1=rot_quat(axis,r);
            test_qvm::matrix<M1,3,3> x1=convert_to< test_qvm::matrix<M1,3,3> >(q1);
            test_qvm::rotation_y(x1.b,r);
            BOOST_QVM_TEST_CLOSE(x1.a,x1.b,0.000001f);
            test_qvm::quaternion<Q2> q2(42,1);
            set_rot(q2,axis,r);
            test_qvm::matrix<M2,3,3> x2=convert_to< test_qvm::matrix<M2,3,3> >(q2);
            test_qvm::rotation_y(x2.b,r);
            BOOST_QVM_TEST_CLOSE(x2.a,x2.b,0.000001f);
            test_qvm::quaternion<Q1> q3(42,1);
            test_qvm::quaternion<Q1> q4(42,1);
            rotate(q3,axis,r);
            q3 = q3*q1;
            BOOST_QVM_TEST_EQ(q3.a,q3.a);
            }
        }

    void
    test_z()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,3> axis; axis.a[2]=1;
        for( float r=0; r<6.28f; r+=0.5f )
            {
            test_qvm::quaternion<Q1> q1=rot_quat(axis,r);
            test_qvm::matrix<M1,3,3> x1=convert_to< test_qvm::matrix<M1,3,3> >(q1);
            test_qvm::rotation_z(x1.b,r);
            BOOST_QVM_TEST_CLOSE(x1.a,x1.b,0.000001f);
            test_qvm::quaternion<Q2> q2(42,1);
            set_rot(q2,axis,r);
            test_qvm::matrix<M2,3,3> x2=convert_to< test_qvm::matrix<M2,3,3> >(q2);
            test_qvm::rotation_z(x2.b,r);
            BOOST_QVM_TEST_CLOSE(x2.a,x2.b,0.000001f);
            test_qvm::quaternion<Q1> q3(42,1);
            test_qvm::quaternion<Q1> q4(42,1);
            rotate(q3,axis,r);
            q3 = q3*q1;
            BOOST_QVM_TEST_EQ(q3.a,q3.a);
            }
        }
    }

int
main()
    {
    test_x();
    test_y();
    test_z();
    return boost::report_errors();
    }
