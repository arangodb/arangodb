//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_mat_operations.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/mat.hpp>
#include <boost/qvm/vec.hpp>
#include "test_qvm_matrix.hpp"
#include "test_qvm_vector.hpp"
#include "gold.hpp"

namespace
    {
    template <class T,class U>
    struct same_type;

    template <class T>
    struct
    same_type<T,T>
        {
        };

    template <class T,class U>
    void
    check_same_type( T const &, U const & )
        {
        same_type<T,U>();
        }

    template <int M,int N>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,M,N> const x(42,1);
        test_qvm::vector<V1,N> const y(42,1);
            {
            test_qvm::vector<V2,M> r=x*y;
            test_qvm::multiply_mv(r.b,x.b,y.b);
            BOOST_QVM_TEST_CLOSE(r.a,r.b,0.0000001f);
            }
            {
            test_qvm::vector<V2,M> r=mref(x)*y;
            test_qvm::multiply_mv(r.b,x.b,y.b);
            BOOST_QVM_TEST_CLOSE(r.a,r.b,0.0000001f);
            }
            {
            test_qvm::vector<V2,M> r=x*vref(y);
            test_qvm::multiply_mv(r.b,x.b,y.b);
            BOOST_QVM_TEST_CLOSE(r.a,r.b,0.0000001f);
            }
        check_same_type(x*y,boost::qvm::vec<float,M>());
        }
    }

int
main()
    {
    test<1,2>();
    test<2,1>();
    test<2,2>();
    test<1,3>();
    test<3,1>();
    test<3,3>();
    test<1,4>();
    test<4,1>();
    test<4,4>();
    test<1,5>();
    test<5,1>();
    test<5,5>();
    return boost::report_errors();
    }
