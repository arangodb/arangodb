//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/quat_operations.hpp>
#endif

#include "test_qvm_quaternion.hpp"
#include "gold.hpp"

namespace
    {
    void
    test()
        {
        using namespace boost::qvm::sfinae;

        test_qvm::quaternion<Q1> const x(42,1);
        float m1=mag(x);
        float m2=mag(qref(x));
        float m3=sqrtf(test_qvm::dot<float>(x.a,x.a));
        BOOST_QVM_TEST_CLOSE(m1,m3,0.000001f);
        BOOST_QVM_TEST_CLOSE(m2,m3,0.000001f);
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
