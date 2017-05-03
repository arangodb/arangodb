//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_operations.hpp>
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <int Dim>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::vector<V1,Dim,double> x(42,1);
        test_qvm::vector<V1,Dim,float> y;
        assign(y,scalar_cast<float>(x));
        for( int i=0; i!=Dim; ++i )
            y.b[i]=static_cast<float>(x.a[i]);
        BOOST_QVM_TEST_EQ(y.a,y.b);
        }
    }

int
main()
    {
    test<2>();
    return boost::report_errors();
    }
