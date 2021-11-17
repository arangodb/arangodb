#include <boost/config.hpp>

#ifndef BOOST_NO_CXX11_DECLTYPE
#  define BOOST_NO_CXX11_DECLTYPE
#  include "has_member_function_callable_with.cpp"
#else
   int main()
   {
      return 0;
   }
#endif

