//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_operations.hpp>
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <class T,class U> struct same_type_tester;
    template <class T> struct same_type_tester<T,T> { };
    template <class T,class U> void test_same_type( T, U ) { same_type_tester<T,U>(); }

    template <int Dim>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
            {
        test_qvm::vector<V1,Dim> const x(42,1);
        test_same_type(x,normalized(x));
        test_qvm::vector<V1,Dim> y=normalized(x);
        float m=sqrtf(test_qvm::dot<float>(x.a,x.a));
        test_qvm::scalar_multiply_v(y.b,x.a,1/m);
        BOOST_QVM_TEST_CLOSE(y.a,y.b,0.000001f);
            }
            {
        test_qvm::vector<V1,Dim> x(42,1);
        float m=sqrtf(test_qvm::dot<float>(x.a,x.a));
        test_qvm::scalar_multiply_v(x.b,x.a,1/m);
        normalize(x);
        BOOST_QVM_TEST_CLOSE(x.a,x.b,0.000001f);
            }
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
