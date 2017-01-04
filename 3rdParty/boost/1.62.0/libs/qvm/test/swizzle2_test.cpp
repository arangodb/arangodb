//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/vec.hpp>
#include <boost/qvm/swizzle.hpp>
#include "test_qvm_vector.hpp"

int
main()
    {       
    using namespace boost::qvm;
    test_qvm::vector<V1,4> v1;
    v1.a[0]=42.0f;
    v1.a[1]=43.0f;
    v1.a[2]=44.0f;
    v1.a[3]=45.0f;
    XX(v1) + XX(v1);
    -XX(v1);
        {
        test_qvm::vector<V2,2> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        test_qvm::vector<V2,2> v2=XX(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,2> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[1];
        test_qvm::vector<V2,2> v2=XY(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,2> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[2];
        test_qvm::vector<V2,2> v2=XZ(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,2> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[3];
        test_qvm::vector<V2,2> v2=XW(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,2> r;
        r.a[0]=v1.a[0];
        r.a[1]=0;
        test_qvm::vector<V2,2> v2=X0(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,2> r;
        r.a[0]=v1.a[0];
        r.a[1]=1;
        test_qvm::vector<V2,2> v2=X1(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,2> v2=XY(v1);
        test_qvm::vector<V3,2> v3;
        XY(v3)=XY(v2);
        BOOST_QVM_TEST_EQ(v2,v3);
        }
        {
        test_qvm::vector<V1,2> v=_00();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==0);
        }
        {
        test_qvm::vector<V1,2> v=_01();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==1);
        }
        {
        test_qvm::vector<V1,2> v=_10();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==0);
        }
        {
        test_qvm::vector<V1,2> v=_11();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==1);
        }
    return boost::report_errors();
    }
