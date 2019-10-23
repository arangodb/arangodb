//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_operations.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/quat.hpp>
#include "test_qvm_quaternion.hpp"
#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <class T,class U> struct same_type_tester;
    template <class T> struct same_type_tester<T,T> { };
    template <class T,class U> void test_same_type( T, U ) { same_type_tester<T,U>(); }

    void
    test()
        {
        using namespace boost::qvm;
        for( float a=0; a<6.28f; a+=0.2f )
            {
            test_qvm::quaternion<Q1> const qx=rotx_quat(a);
            test_qvm::quaternion<Q1> const qy=roty_quat(a);
            test_qvm::quaternion<Q1> const qz=rotz_quat(a);
            test_qvm::quaternion<Q1> const q1=qx*qy*qref(qz);
            test_qvm::matrix<M1,3,3> const mx=rotx_mat<3>(a);
            test_qvm::matrix<M1,3,3> const my=roty_mat<3>(a);
            test_qvm::matrix<M1,3,3> const mz=rotz_mat<3>(a);
            test_qvm::matrix<M1,3,3> const m=mx*my*mz;
            test_qvm::quaternion<Q1> const q2=convert_to< test_qvm::quaternion<Q1> >(m);
            test_same_type(qx,qx*qy);
            BOOST_QVM_TEST_CLOSE_QUAT(q1.a,q2.a,0.00001f);
            }
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
