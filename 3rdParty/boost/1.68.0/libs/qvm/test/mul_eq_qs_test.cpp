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
        test_qvm::quaternion<Q1> x(42,1);
        test_qvm::scalar_multiply_v(x.b,x.a,2);
        x*=2;
        BOOST_QVM_TEST_EQ(x.a,x.b);
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
