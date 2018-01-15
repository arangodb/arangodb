//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Qersion 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_operations.hpp>
#include "test_qvm_quaternion.hpp"
#include "gold.hpp"
#include <functional>

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
                BOOST_TEST(cmp(x,y,std::equal_to<float>()));
                y.a[i]=0;
                BOOST_TEST(!cmp(x,y,std::equal_to<float>()));
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
