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
    template <class T>
    void
    test_perspective_lh( T fov_y, T aspect_ratio, T z_near, T z_far )
        {
        using namespace boost::qvm;
        test_qvm::matrix<M1,4,4> const m=perspective_lh(fov_y,aspect_ratio,z_near,z_far);
        test_qvm::matrix_perspective_lh(m.b,fov_y,aspect_ratio,z_near,z_far);
        BOOST_QVM_TEST_CLOSE(m.a,m.b,0.000001f);
        }

    template <class T>
    void
    test_perspective_rh( T fov_y, T aspect_ratio, T z_near, T z_far )
        {
        using namespace boost::qvm;
        test_qvm::matrix<M1,4,4> const m=perspective_rh(fov_y,aspect_ratio,z_near,z_far);
        test_qvm::matrix_perspective_rh(m.b,fov_y,aspect_ratio,z_near,z_far);
        BOOST_QVM_TEST_CLOSE(m.a,m.b,0.000001f);
        }
    }

int
main()
    {
    test_perspective_lh(0.5f,1.3f,0.1f,2000.0f);
    test_perspective_rh(0.5f,1.3f,0.1f,2000.0f);
    return boost::report_errors();
    }
