//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_operations.hpp>
#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <int D>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,D,D> const x(42,1);
        float gd=test_qvm::determinant(x.b);
        float d=determinant(x);
        BOOST_QVM_TEST_EQ(gd,d);
        }
    }

int
main()
    {
    test<2>();
    test<3>();
    test<4>();
    test<5>();
    return boost::report_errors();
    }
