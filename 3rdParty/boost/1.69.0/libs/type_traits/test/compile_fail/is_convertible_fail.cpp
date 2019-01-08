
//  (C) Copyright John Maddock 2017. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#  include <boost/type_traits/is_convertible.hpp>
#include "../test.hpp"

int main()
{
   return boost::is_convertible<UDT, incomplete_type>::value;
}


