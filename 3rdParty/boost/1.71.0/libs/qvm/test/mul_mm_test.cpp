//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/mat.hpp>
#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <int R,int CR,int C>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,R,CR> const x(42,1);
        test_qvm::matrix<M2,CR,C> const y(42,1);
            {
            test_qvm::matrix<M3,R,C> r=x*y;
            test_qvm::multiply_m(r.b,x.b,y.b);
            BOOST_QVM_TEST_CLOSE(r.a,r.b,0.0000001f);
            }
            {
            test_qvm::matrix<M3,R,C> r=mref(x)*y;
            test_qvm::multiply_m(r.b,x.b,y.b);
            BOOST_QVM_TEST_CLOSE(r.a,r.b,0.0000001f);
            }
            {
            test_qvm::matrix<M3,R,C> r=x*mref(y);
            test_qvm::multiply_m(r.b,x.b,y.b);
            BOOST_QVM_TEST_CLOSE(r.a,r.b,0.0000001f);
            }
        }
    }

int
main()
    {
    test<1,2,2>();
    test<2,2,1>();
    test<2,2,2>();
    test<1,3,3>();
    test<3,3,1>();
    test<3,3,3>();
    test<1,4,4>();
    test<4,4,1>();
    test<4,4,4>();
    test<1,5,5>();
    test<5,5,1>();
    test<5,5,5>();
    test<2,3,4>();
    return boost::report_errors();
    }
