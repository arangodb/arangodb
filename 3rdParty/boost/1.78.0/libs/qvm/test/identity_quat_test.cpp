//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/quat_operations.hpp>
#endif

#include "test_qvm_quaternion.hpp"

namespace
    {
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::quaternion<Q1> q=identity_quat<float>();
        BOOST_TEST(q.a[0]==1);
        BOOST_TEST(q.a[1]==0);
        BOOST_TEST(q.a[2]==0);
        BOOST_TEST(q.a[3]==0);
        test_qvm::quaternion<Q2> p(42,1);
        set_identity(p);
        BOOST_TEST(p.a[0]==1);
        BOOST_TEST(p.a[1]==0);
        BOOST_TEST(p.a[2]==0);
        BOOST_TEST(p.a[3]==0);
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
