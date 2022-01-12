//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/mat_access.hpp>
#endif

#include "test_qvm_matrix.hpp"

using namespace boost::qvm;

template <int R,int C>
void
check_idx( test_qvm::matrix<M1,10,10> & m, float & (*f)( test_qvm::matrix<M1,10,10> & ) )
    {
    BOOST_TEST((&A<R,C>(m)==&m.a[R][C]));
    BOOST_TEST((&f(m)==&m.a[R][C]));
    }

int
main()
    {       
    test_qvm::matrix<M1,10,10> m;
#define CHECK_A(i,j) check_idx<i,j>(m,A##i##j);
    CHECK_A(0,0);
    CHECK_A(0,1);
    CHECK_A(0,2);
    CHECK_A(0,3);
    CHECK_A(0,4);
    CHECK_A(0,5);
    CHECK_A(0,6);
    CHECK_A(0,7);
    CHECK_A(0,8);
    CHECK_A(0,9);
    CHECK_A(1,0);
    CHECK_A(1,1);
    CHECK_A(1,2);
    CHECK_A(1,3);
    CHECK_A(1,4);
    CHECK_A(1,5);
    CHECK_A(1,6);
    CHECK_A(1,7);
    CHECK_A(1,8);
    CHECK_A(1,9);
    CHECK_A(2,0);
    CHECK_A(2,1);
    CHECK_A(2,2);
    CHECK_A(2,3);
    CHECK_A(2,4);
    CHECK_A(2,5);
    CHECK_A(2,6);
    CHECK_A(2,7);
    CHECK_A(2,8);
    CHECK_A(2,9);
    CHECK_A(3,0);
    CHECK_A(3,1);
    CHECK_A(3,2);
    CHECK_A(3,3);
    CHECK_A(3,4);
    CHECK_A(3,5);
    CHECK_A(3,6);
    CHECK_A(3,7);
    CHECK_A(3,8);
    CHECK_A(3,9);
    CHECK_A(4,0);
    CHECK_A(4,1);
    CHECK_A(4,2);
    CHECK_A(4,3);
    CHECK_A(4,4);
    CHECK_A(4,5);
    CHECK_A(4,6);
    CHECK_A(4,7);
    CHECK_A(4,8);
    CHECK_A(4,9);
    CHECK_A(5,0);
    CHECK_A(5,1);
    CHECK_A(5,2);
    CHECK_A(5,3);
    CHECK_A(5,4);
    CHECK_A(5,5);
    CHECK_A(5,6);
    CHECK_A(5,7);
    CHECK_A(5,8);
    CHECK_A(5,9);
    CHECK_A(6,0);
    CHECK_A(6,1);
    CHECK_A(6,2);
    CHECK_A(6,3);
    CHECK_A(6,4);
    CHECK_A(6,5);
    CHECK_A(6,6);
    CHECK_A(6,7);
    CHECK_A(6,8);
    CHECK_A(6,9);
    CHECK_A(7,0);
    CHECK_A(7,1);
    CHECK_A(7,2);
    CHECK_A(7,3);
    CHECK_A(7,4);
    CHECK_A(7,5);
    CHECK_A(7,6);
    CHECK_A(7,7);
    CHECK_A(7,8);
    CHECK_A(7,9);
    CHECK_A(8,0);
    CHECK_A(8,1);
    CHECK_A(8,2);
    CHECK_A(8,3);
    CHECK_A(8,4);
    CHECK_A(8,5);
    CHECK_A(8,6);
    CHECK_A(8,7);
    CHECK_A(8,8);
    CHECK_A(8,9);
    CHECK_A(9,0);
    CHECK_A(9,1);
    CHECK_A(9,2);
    CHECK_A(9,3);
    CHECK_A(9,4);
    CHECK_A(9,5);
    CHECK_A(9,6);
    CHECK_A(9,7);
    CHECK_A(9,8);
    CHECK_A(9,9);
#undef CHECK_A
    return boost::report_errors();
    }
