//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_operations.hpp>
#include <boost/qvm/vec_traits_array.hpp>
#include "test_qvm_vector.hpp"
#include "gold.hpp"
#include <functional>

namespace
    {
    template <int Dim>
    void
    test()
        {
        using namespace boost::qvm::sfinae;
        test_qvm::vector<V1,Dim> const x(42,1);
        for( int i=0; i!=Dim; ++i )
            {
                {
                test_qvm::vector<V1,Dim> y(x);
                BOOST_TEST(cmp(x,y,std::equal_to<float>()));
                y.a[i]=0;
                BOOST_TEST(!cmp(x,y,std::equal_to<float>()));
                }
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
