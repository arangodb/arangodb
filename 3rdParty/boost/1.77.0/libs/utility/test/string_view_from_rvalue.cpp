/*
   Copyright (c) Marshall Clow 2017.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <iostream>
#include <algorithm>
#include <string>

#include <boost/utility/string_view.hpp>

#if defined(BOOST_NO_CXX11_RVALUE_REFERENCES) || defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
#error "Unsupported test"
#endif

std::string makeatemp() { return "abc"; }

int main()
{
  boost::basic_string_view<char> sv(makeatemp());
  return 0;
}
