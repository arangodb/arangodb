//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_operations.hpp>
#include "test_qvm_quaternion.hpp"
#include "gold.hpp"

namespace
    {
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::quaternion<Q1> const x(42,1);
        float m1=mag_sqr(x);
        float m2=mag_sqr(qref(x));
        float m3=test_qvm::dot<float>(x.a,x.a);
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
