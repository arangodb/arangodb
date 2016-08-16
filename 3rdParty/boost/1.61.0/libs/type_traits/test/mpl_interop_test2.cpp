
//  (C) Copyright John Maddock 2000. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_traits/is_void.hpp>
#include <boost/mpl/if.hpp>
#include <boost/static_assert.hpp>

template <class T>
struct if_test
{
   typedef typename boost::mpl::if_<
      boost::is_void<T>,
      int, T>::type type;
};

if_test<void>::type t1 = 0;
if_test<double>::type t2 = 0;

int main()
{
   return (int)(t1 + t2);
}