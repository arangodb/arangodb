//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_access.hpp>
#include "test_qvm_vector.hpp"

using namespace boost::qvm;

template <int I>
void
check_idx( test_qvm::vector<V1,10> & v, float & (*f)( test_qvm::vector<V1,10> & ) )
    {
    BOOST_TEST((&A<I>(v)==&v.a[I]));
    BOOST_TEST((&f(v)==&v.a[I]));
    }

int
main()
    {       
    test_qvm::vector<V1,10> v;
#define CHECK_A(i) check_idx<i>(v,A##i);
    CHECK_A(0);
    CHECK_A(1);
    CHECK_A(2);
    CHECK_A(3);
    CHECK_A(4);
    CHECK_A(5);
    CHECK_A(6);
    CHECK_A(7);
    CHECK_A(8);
    CHECK_A(9);
#undef CHECK_A
    BOOST_TEST(&A<0>(v)==&X(v));
    BOOST_TEST(&A<1>(v)==&Y(v));
    BOOST_TEST(&A<2>(v)==&Z(v));
    BOOST_TEST(&A<3>(v)==&W(v));
    return boost::report_errors();
    }
