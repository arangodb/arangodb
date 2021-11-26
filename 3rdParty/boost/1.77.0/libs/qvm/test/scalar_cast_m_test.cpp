//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/mat_operations.hpp>
#endif

#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <int Rows,int Cols>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,Rows,Cols,double> x(42,1);
        test_qvm::matrix<M1,Rows,Cols,float> y;
        assign(y,scalar_cast<float>(x));
        for( int i=0; i!=Rows; ++i )
            for( int j=0; j!=Cols; ++j )
                y.b[i][j]=static_cast<float>(x.a[i][j]);
        BOOST_QVM_TEST_EQ(y.a,y.b);
        }
    }

int
main()
    {
    test<1,2>();
    test<2,1>();
    test<2,2>();
    return boost::report_errors();
    }
