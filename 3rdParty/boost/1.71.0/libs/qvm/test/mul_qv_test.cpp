//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/operations.hpp>
#include <boost/qvm/vec.hpp>
#include "test_qvm_quaternion.hpp"
#include "test_qvm_matrix.hpp"
#include "test_qvm_vector.hpp"
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
            test_qvm::matrix<M1,3,3> const mx=rotx_mat<3>(a);
            test_qvm::matrix<M1,3,3> const my=roty_mat<3>(a);
            test_qvm::matrix<M1,3,3> const mz=rotz_mat<3>(a);
            test_qvm::vector<V1,3> const v(42,1);
            test_same_type(vec<float,3>(),qx*v);
            test_qvm::vector<V1,3> const q_vx=qx*v;
            test_qvm::vector<V1,3> const m_vx=mx*v;
            test_qvm::vector<V1,3> const q_vy=qy*v;
            test_qvm::vector<V1,3> const m_vy=my*v;
            test_qvm::vector<V1,3> const q_vz=qz*v;
            test_qvm::vector<V1,3> const m_vz=mz*v;
            BOOST_QVM_TEST_CLOSE(q_vx.a,m_vx.a,0.001f);
            BOOST_QVM_TEST_CLOSE(q_vy.a,m_vy.a,0.001f);
            BOOST_QVM_TEST_CLOSE(q_vz.a,m_vz.a,0.001f);
            }
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
