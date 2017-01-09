//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_index.hpp>
#include "test_qvm_matrix.hpp"

int
main()
    {       
    using namespace boost::qvm;

    test_qvm::matrix<M1,2,3> m;
    m.a[0][0]=42.0f;
    m.a[0][1]=43.0f;
    m.a[0][2]=44.0f;
    m.a[1][0]=45.0f;
    m.a[1][1]=46.0f;
    m.a[1][2]=47.0f;
    BOOST_TEST(mat_index_read(m,0,0)==m.a[0][0]);
    BOOST_TEST(mat_index_read(m,0,1)==m.a[0][1]);
    BOOST_TEST(mat_index_read(m,0,2)==m.a[0][2]);
    BOOST_TEST(mat_index_read(m,1,0)==m.a[1][0]);
    BOOST_TEST(mat_index_read(m,1,1)==m.a[1][1]);
    BOOST_TEST(mat_index_read(m,1,2)==m.a[1][2]);
    BOOST_TEST(&mat_index_write(m,0,0)==&m.a[0][0]);
    BOOST_TEST(&mat_index_write(m,0,1)==&m.a[0][1]);
    BOOST_TEST(&mat_index_write(m,0,2)==&m.a[0][2]);
    BOOST_TEST(&mat_index_write(m,1,0)==&m.a[1][0]);
    BOOST_TEST(&mat_index_write(m,1,1)==&m.a[1][1]);
    BOOST_TEST(&mat_index_write(m,1,2)==&m.a[1][2]);
    return boost::report_errors();
    }
