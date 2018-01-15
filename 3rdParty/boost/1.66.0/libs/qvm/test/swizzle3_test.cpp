//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/vec_access.hpp>
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
    XXX(v1) + XXX(v1);
    -XXX(v1);
    XXX(42.0f) + XXX(42.0f);
    -XXX(42.0f);
        {
        test_qvm::vector<V2,3> v0=X00(42.0f);
        BOOST_TEST(v0.a[0]==42);
        BOOST_TEST(v0.a[1]==0);
        BOOST_TEST(v0.a[2]==0);
        test_qvm::vector<V2,3> v2=_10X(42.0f);
        BOOST_TEST(v2.a[0]==1);
        BOOST_TEST(v2.a[1]==0);
        BOOST_TEST(v2.a[2]==42);
        float s=42.0f;
        BOOST_TEST(&X(X01(s))==&s);
        }
        {
        test_qvm::vector<V2,3> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[0];
        test_qvm::vector<V2,3> v2=XXX(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,3> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[1];
        test_qvm::vector<V2,3> v2=XXY(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,3> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[2];
        test_qvm::vector<V2,3> v2=XXZ(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,3> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[3];
        test_qvm::vector<V2,3> v2=XXW(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,3> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=0;
        test_qvm::vector<V2,3> v2=XX0(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,3> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=1;
        test_qvm::vector<V2,3> v2=XX1(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,3> v2=XYZ(v1);
        XYZ(v1) *= 2;
        v2 *= 2;
        test_qvm::vector<V2,3> v3=XYZ(v1);
        BOOST_QVM_TEST_EQ(v2,v3);
        }
        {
        test_qvm::vector<V2,3> v2=XYZ(v1);
        test_qvm::vector<V3,3> v3;
        XYZ(v3)=XYZ(v2);
        BOOST_QVM_TEST_EQ(v2,v3);
        }
        {
        test_qvm::vector<V1,3> v=_000();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==0);
        }
        {
        test_qvm::vector<V1,3> v=_001();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==1);
        }
        {
        test_qvm::vector<V1,3> v=_010();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==0);
        }
        {
        test_qvm::vector<V1,3> v=_011();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==1);
        }
        {
        test_qvm::vector<V1,3> v=_100();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==0);
        }
        {
        test_qvm::vector<V1,3> v=_101();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==1);
        }
        {
        test_qvm::vector<V1,3> v=_110();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==0);
        }
        {
        test_qvm::vector<V1,3> v=_111();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==1);
        }
    return boost::report_errors();
    }
