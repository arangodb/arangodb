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

        test_qvm::vector<V1,Dim> const x(42,1);
            {
            test_qvm::vector<V1,Dim> const y(43,1);
            test_same_type(float(),dot(x,y));
            float d1=dot(x,y);
            float d2=test_qvm::dot<float>(x.a,y.a);
            BOOST_QVM_TEST_CLOSE(d1,d2,0.000001f);
            }
            {
            test_qvm::vector<V1,Dim,double> const y(43,1);
            test_same_type(double(),dot(x,y));
            double d1=dot(x,y);
            double d2=test_qvm::dot<double>(x.a,y.a);
            BOOST_QVM_TEST_CLOSE(d1,d2,0.000001);
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
