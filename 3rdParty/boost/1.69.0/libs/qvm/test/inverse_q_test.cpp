//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_operations.hpp>
#include <boost/qvm/quat.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include "test_qvm_quaternion.hpp"
#include "test_qvm.hpp"

namespace
    {
    template <class T,class U> struct same_type_tester;
    template <class T> struct same_type_tester<T,T> { };
    template <class T,class U> void test_same_type( T, U ) { same_type_tester<T,U>(); }

    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::quaternion<Q1> x=rotx_quat(4.2f);
        test_same_type(x,inverse(x));
        test_qvm::quaternion<Q1> y=rotx_quat(-4.2f);
            {
            test_qvm::quaternion<Q1> z=inverse(x);
            BOOST_QVM_TEST_CLOSE_QUAT(z.a,y.a,0.00001f);
            }
            {
            test_qvm::quaternion<Q1> z=inverse(qref(x));
            BOOST_QVM_TEST_CLOSE_QUAT(z.a,y.a,0.00001f);
            }
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
