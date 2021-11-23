//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_QVM_TEST_SINGLE_HEADER
#   include BOOST_QVM_TEST_SINGLE_HEADER
#else
#   include <boost/qvm/vec_operations.hpp>
#   include <boost/qvm/vec.hpp>
#endif

#include "test_qvm_vector.hpp"

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
    check_deduction( T const &, U const & )
        {
        same_type<T,typename boost::qvm::deduce_vec<U>::type>();
        }

    template <int Dim>
    void
    test()
        {
        using namespace boost::qvm;
        test_qvm::vector<V1,Dim> v1=zero_vec<float,Dim>();
        for( int i=0; i!=Dim; ++i )
                BOOST_TEST(!v1.a[i]);
        test_qvm::vector<V2,Dim> v2(42,1);
        set_zero(v2);
        for( int i=0; i!=Dim; ++i )
                BOOST_TEST(!v2.a[i]);
        check_deduction(vec<float,Dim>(),zero_vec<float,Dim>());
        check_deduction(vec<int,Dim>(),zero_vec<int,Dim>());
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
