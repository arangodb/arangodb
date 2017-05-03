//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_operations.hpp>
#include "test_qvm_quaternion.hpp"

namespace
    {
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::quaternion<Q1> v1=zero_quat<float>();
        for( int i=0; i!=4; ++i )
                BOOST_TEST(!v1.a[i]);
        test_qvm::quaternion<Q2> v2(42,1);
        set_zero(v2);
        for( int i=0; i!=4; ++i )
                BOOST_TEST(!v2.a[i]);
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
