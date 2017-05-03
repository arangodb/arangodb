//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/to_string.hpp>
#include <boost/qvm/quat_operations.hpp>
#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/mat_operations.hpp>
#include "test_qvm_matrix.hpp"
#include "test_qvm_quaternion.hpp"
#include "test_qvm_vector.hpp"

namespace
    {
    template <int Rows,int Cols>
    void
    test_matrix( std::string const & gold )
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,Rows,Cols,int> const x(42,1);
        std::string s=to_string(x);
        BOOST_TEST(s==gold);
        }

    template <int Dim>
    void
    test_vector( std::string const & gold )
        {
        using namespace boost::qvm::sfinae;
        test_qvm::vector<V1,Dim,int> const x(42,1);
        std::string s=to_string(x);
        BOOST_TEST(s==gold);
        }

    void
    test_quaternion( std::string const & gold )
        {
        using namespace boost::qvm::sfinae;
        test_qvm::quaternion<Q1,int> const x(42,1);
        std::string s=to_string(x);
        BOOST_TEST(s==gold);
        }
    }

int
main()
    {
    test_matrix<1,2>("((42,43))");
    test_matrix<2,1>("((42)(43))");
    test_matrix<2,2>("((42,43)(44,45))");
    test_matrix<1,3>("((42,43,44))");
    test_matrix<3,1>("((42)(43)(44))");
    test_matrix<3,3>("((42,43,44)(45,46,47)(48,49,50))");
    test_matrix<1,4>("((42,43,44,45))");
    test_matrix<4,1>("((42)(43)(44)(45))");
    test_matrix<4,4>("((42,43,44,45)(46,47,48,49)(50,51,52,53)(54,55,56,57))");
    test_matrix<1,5>("((42,43,44,45,46))");
    test_matrix<5,1>("((42)(43)(44)(45)(46))");
    test_matrix<5,5>("((42,43,44,45,46)(47,48,49,50,51)(52,53,54,55,56)(57,58,59,60,61)(62,63,64,65,66))");
    test_vector<2>("(42,43)");
    test_vector<3>("(42,43,44)");
    test_vector<4>("(42,43,44,45)");
    test_vector<5>("(42,43,44,45,46)");
    test_quaternion("(42,43,44,45)");
    return boost::report_errors();
    }
