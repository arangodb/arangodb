//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#   ifdef BOOST_QVM_TEST_SINGLE_HEADER_SWIZZLE
#       include BOOST_QVM_TEST_SINGLE_HEADER_SWIZZLE
#   endif
#else
#   include <boost/qvm/vec_operations.hpp>
#   include <boost/qvm/vec_access.hpp>
#   include <boost/qvm/vec.hpp>
#   include <boost/qvm/swizzle.hpp>
#endif

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
    XXXX(v1) + XXXX(v1);
    -XXXX(v1);
    XXXX(42.0f) + XXXX(42.0f);
    -XXXX(42.0f);
        {
        test_qvm::vector<V2,4> v0=X001(42.0f);
        BOOST_TEST(v0.a[0]==42);
        BOOST_TEST(v0.a[1]==0);
        BOOST_TEST(v0.a[2]==0);
        BOOST_TEST(v0.a[3]==1);
        test_qvm::vector<V2,4> v2=_100X(42.0f);
        BOOST_TEST(v2.a[0]==1);
        BOOST_TEST(v2.a[1]==0);
        BOOST_TEST(v2.a[2]==0);
        BOOST_TEST(v2.a[3]==42);
        float s=42.0f;
        BOOST_TEST(&X(X101(s))==&s);
        }
        {
        test_qvm::vector<V2,4> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[0];
        r.a[3]=v1.a[0];
        test_qvm::vector<V2,4> v2=XXXX(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,4> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[0];
        r.a[3]=v1.a[1];
        test_qvm::vector<V2,4> v2=XXXY(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,4> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[0];
        r.a[3]=v1.a[2];
        test_qvm::vector<V2,4> v2=XXXZ(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,4> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[0];
        r.a[3]=v1.a[3];
        test_qvm::vector<V2,4> v2=XXXW(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,4> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[0];
        r.a[3]=0;
        test_qvm::vector<V2,4> v2=XXX0(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,4> r;
        r.a[0]=v1.a[0];
        r.a[1]=v1.a[0];
        r.a[2]=v1.a[0];
        r.a[3]=1;
        test_qvm::vector<V2,4> v2=XXX1(v1);
        BOOST_QVM_TEST_EQ(v2,r);
        }
        {
        test_qvm::vector<V2,4> v2=XYZW(v1);
        XYZW(v1) *= 2;
        v2 *= 2;
        test_qvm::vector<V2,4> v3=XYZW(v1);
        BOOST_QVM_TEST_EQ(v2,v3);
        }
        {
        test_qvm::vector<V2,4> v2=XYZW(v1);
        test_qvm::vector<V3,4> v3;
        XYZW(v3)=XYZW(v2);
        BOOST_QVM_TEST_EQ(v2,v3);
        }
        {
        test_qvm::vector<V1,4> v=_0000();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_0001();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==1);
        }
        {
        test_qvm::vector<V1,4> v=_0010();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_0011();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==1);
        }
        {
        test_qvm::vector<V1,4> v=_0100();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_0101();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==1);
        }
        {
        test_qvm::vector<V1,4> v=_0110();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_0111();
        BOOST_TEST(v.a[0]==0);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==1);
        }
        {
        test_qvm::vector<V1,4> v=_1000();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_1001();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==1);
        }
        {
        test_qvm::vector<V1,4> v=_1010();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_1011();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==0);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==1);
        }
        {
        test_qvm::vector<V1,4> v=_1100();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_1101();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==0);
        BOOST_TEST(v.a[3]==1);
        }
        {
        test_qvm::vector<V1,4> v=_1110();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==0);
        }
        {
        test_qvm::vector<V1,4> v=_1111();
        BOOST_TEST(v.a[0]==1);
        BOOST_TEST(v.a[1]==1);
        BOOST_TEST(v.a[2]==1);
        BOOST_TEST(v.a[3]==1);
        }
    return boost::report_errors();
    }
