//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/map.hpp>
#include <boost/qvm/mat_traits_array.hpp>
#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/mat.hpp>
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <int Dim>
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,Dim> x(42,1);
        float y[Dim][Dim]; assign(y,diag_mat(x));
        for( int i=0; i!=Dim; ++i )
            x.b[i]=y[i][i];
        BOOST_QVM_TEST_EQ(x.a,x.b);
        test_qvm::scalar_multiply_v(x.b,x.a,2.0f);
        diag(diag_mat(x)) *= 2;
        BOOST_QVM_TEST_EQ(x.a,x.b);
        diag_mat(x) + diag_mat(x);
        -diag_mat(x);
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
