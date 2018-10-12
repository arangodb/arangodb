//  Copyright Nick Thompson 2017.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/math/concepts/std_real_concept.hpp>
#include <boost/math/tools/numerical_differentiation.hpp>


void compile_and_link_test()
{
   boost::math::concepts::std_real_concept x = 0;
   auto f = [](boost::math::concepts::std_real_concept x) { return x; };
   boost::math::tools::finite_difference_derivative(f, x);
}
