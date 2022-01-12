//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/mat_operations.hpp>
#   include <boost/qvm/mat.hpp>
#endif

#include <boost/qvm/mat_traits_array.hpp>
#include "test_qvm_matrix.hpp"
#include "gold.hpp"

namespace
    {
    template <int Rows,int Cols>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::matrix<M1,Rows,Cols> const x(42,1);
        for( int i=0; i!=Rows; ++i )
            for( int j=0; j!=Cols; ++j )
                {
                    {
                    test_qvm::matrix<M1,Rows,Cols> y(x);
                    BOOST_QVM_TEST_EQ(x,y);
                    y.a[i][j]=0;
                    BOOST_QVM_TEST_NEQ(x,y);
                    }
                    {
                    test_qvm::matrix<M2,Rows,Cols> y; assign(y,x);
                    BOOST_QVM_TEST_EQ(x,y);
                    y.a[i][j]=0;
                    BOOST_QVM_TEST_NEQ(x,y);
                    }
                }
        }

    template <class T>
    struct test_scalar
    {
        T value_;
        test_scalar( T value ): value_(value) {}
    }; //No operator==

    struct
    equal_to
    {
        template <class T,class U>
        bool
        operator()( T const & a, U const & b )
        {
            return a.value_==b.value_;
        }
    };

    template <class A, class B>
    void
    test2()
        {
        typedef test_scalar<A> scalar_a;
        typedef test_scalar<B> scalar_b;
        typedef boost::qvm::mat<scalar_a, 3, 3> mat_a;
        typedef boost::qvm::mat<scalar_b, 3, 3> mat_b;
        mat_a const a = { { {42, 94, 96}, {72, 95, 81}, {12, 84, 33} } };
        mat_b const b = { { {42, 94, 96}, {72, 95, 81}, {12, 84, 33} } };
        mat_a const c = { { {21, 47, 48}, {36, 47, 65}, {79, 27, 41} } };
        mat_b const d = { { {21, 47, 48}, {36, 47, 65}, {79, 27, 41} } };
        BOOST_TEST(cmp(a,a,equal_to()));
        BOOST_TEST(cmp(a,b,equal_to()));
        BOOST_TEST(cmp(b,a,equal_to()));
        BOOST_TEST(cmp(b,b,equal_to()));
        BOOST_TEST(cmp(c,c,equal_to()));
        BOOST_TEST(cmp(c,d,equal_to()));
        BOOST_TEST(cmp(d,c,equal_to()));
        BOOST_TEST(cmp(d,d,equal_to()));
        BOOST_TEST(!cmp(a,c,equal_to()));
        BOOST_TEST(!cmp(c,a,equal_to()));
        BOOST_TEST(!cmp(a,d,equal_to()));
        BOOST_TEST(!cmp(d,a,equal_to()));
        BOOST_TEST(!cmp(b,c,equal_to()));
        BOOST_TEST(!cmp(c,b,equal_to()));
        BOOST_TEST(!cmp(b,d,equal_to()));
        BOOST_TEST(!cmp(d,b,equal_to()));
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
    test2<int, int>();
    test2<int, double>();
    test2<double, int>();
    test2<double, double>();
    return boost::report_errors();
    }
