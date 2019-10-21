//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/vec.hpp>
#include <boost/qvm/vec_mat_operations.hpp>
#include <boost/qvm/vec_access.hpp>
#include <boost/qvm/map_mat_mat.hpp>
#include <boost/qvm/map_mat_vec.hpp>
#include <boost/qvm/swizzle.hpp>
#include "test_qvm_matrix.hpp"
#include "test_qvm_vector.hpp"
#include "test_qvm.hpp"

namespace
    {
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::matrix<M1,4,4> m=rot_mat<4>(test_qvm::vector<V1,3>(1,0),1.0f);
        X(col<3>(m)) = 42;
        Y(col<3>(m)) = 42;
        Z(col<3>(m)) = 42;
        test_qvm::vector<V1,3> v(42,1);
        test_qvm::vector<V1,3> mv=transform_vector(m,v);
        test_qvm::vector<V1,3> mp=transform_point(m,v);
        test_qvm::vector<V1,3> v3=del_row_col<3,3>(m) * v;
        test_qvm::vector<V1,3> v4=XYZ(m*XYZ1(v));
        BOOST_QVM_TEST_EQ(mv.a,v3.a);
        BOOST_QVM_TEST_EQ(mp.a,v4.a);
        }
    }

int
main()
    {
    test();
    return boost::report_errors();
    }
