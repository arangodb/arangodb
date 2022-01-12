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
        for( int i=0; i!=4; ++i )
            {
                {
                test_qvm::quaternion<Q1> y(x);
                BOOST_QVM_TEST_EQ(x,y);
                y.a[i]=0;
                BOOST_QVM_TEST_NEQ(x,y);
                }
                {
                test_qvm::quaternion<Q2> y; assign(y,x);
                BOOST_QVM_TEST_EQ(x,y);
                y.a[i]=0;
                BOOST_QVM_TEST_NEQ(x,y);
                }
            }
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
