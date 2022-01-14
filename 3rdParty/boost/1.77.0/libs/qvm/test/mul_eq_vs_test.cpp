//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/vec_operations.hpp>
#endif

#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <int Dim>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::vector<V1,Dim> x(42,1);
        test_qvm::scalar_multiply_v(x.b,x.a,2.0f);
        x*=2;
        BOOST_QVM_TEST_EQ(x.a,x.b);
        }
    }

int
main()
    {
    test<1>();
    test<2>();
    test<3>();
    test<4>();
    test<5>();
    return boost::report_errors();
    }
