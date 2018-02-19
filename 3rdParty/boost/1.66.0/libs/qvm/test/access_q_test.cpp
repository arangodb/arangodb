//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_access.hpp>
#include <boost/qvm/vec_access.hpp>
#include "test_qvm_quaternion.hpp"

int
main()
    {       
    using namespace boost::qvm;

    test_qvm::quaternion<Q1> q;
    q.a[0]=42.0f;
    q.a[1]=43.0f;
    q.a[2]=44.0f;
    q.a[3]=45.0f;
    test_qvm::quaternion<Q1> const & qq=q;

    BOOST_TEST_EQ(X(V(qq)),q.a[1]);
    BOOST_TEST_EQ(Y(V(qq)),q.a[2]);
    BOOST_TEST_EQ(Z(V(qq)),q.a[3]);
    BOOST_TEST(&X(V(q))==&q.a[1]);
    BOOST_TEST(&Y(V(q))==&q.a[2]);
    BOOST_TEST(&Z(V(q))==&q.a[3]);

    BOOST_TEST_EQ(S(qq),q.a[0]);
    BOOST_TEST_EQ(X(qq),q.a[1]);
    BOOST_TEST_EQ(Y(qq),q.a[2]);
    BOOST_TEST_EQ(Z(qq),q.a[3]);
    BOOST_TEST(&S(q)==&q.a[0]);
    BOOST_TEST(&X(q)==&q.a[1]);
    BOOST_TEST(&Y(q)==&q.a[2]);
    BOOST_TEST(&Z(q)==&q.a[3]);

    return boost::report_errors();
    }
