//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_operations.hpp>
#include <boost/qvm/quat.hpp>
#include "test_qvm_quaternion.hpp"
#include "gold.hpp"

namespace
    {
    template <class T,class U> struct same_type_tester;
    template <class T> struct same_type_tester<T,T> { };
    template <class T,class U> void test_same_type( T, U ) { same_type_tester<T,U>(); }

    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::quaternion<Q1> const x(42,1);
        test_qvm::scalar_multiply_v(x.b,x.a,-1.0f);
        x.b[0]=-x.b[0];
        test_same_type(x,conjugate(x));
            {
            test_qvm::quaternion<Q1> y=conjugate(x);
            BOOST_QVM_TEST_EQ(x.b,y.a);
            }
            {
            test_qvm::quaternion<Q1> y=conjugate(qref(x));
            BOOST_QVM_TEST_EQ(x.b,y.a);
            }
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
