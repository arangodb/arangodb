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
        test_qvm::quaternion<Q1> const x(42,2);
            {
            test_qvm::quaternion<Q1> const y(42,1);
            test_same_type(x,x+y);
            test_qvm::quaternion<Q1> r=x+y;
            test_qvm::add_v(r.b,x.b,y.b);
            BOOST_QVM_TEST_EQ(r.a,r.b);
            }
            {
            test_qvm::quaternion<Q1> const y(42,1);
            test_qvm::quaternion<Q2> r=qref(x)+y;
            test_qvm::add_v(r.b,x.b,y.b);
            BOOST_QVM_TEST_EQ(r.a,r.b);
            }
            {
            test_qvm::quaternion<Q1> const y(42,1);
            test_qvm::quaternion<Q2> r=x+qref(y);
            test_qvm::add_v(r.b,x.b,y.b);
            BOOST_QVM_TEST_EQ(r.a,r.b);
            }
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
