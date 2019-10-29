
//  (C) Copyright John Maddock 2017. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#  include <boost/type_traits/is_list_constructible.hpp>
#include "../test.hpp"

#ifndef BOOST_TT_HAS_WORKING_IS_COMPLETE
#error "Sorry can't test this."
#endif

int main()
{
   return boost::is_list_constructible<incomplete_type>::value;
}


