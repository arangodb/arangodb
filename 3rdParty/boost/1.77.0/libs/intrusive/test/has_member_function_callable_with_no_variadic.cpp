#include <boost/config.hpp>

#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
#  define BOOST_NO_CXX11_VARIADIC_TEMPLATES
#  include "has_member_function_callable_with.cpp"
#else
   int main()
   {
      return 0;
   }
#endif

