// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at: akrzemi1@gmail.com


#include "boost/optional/optional.hpp"

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#include <string>
#include "boost/core/addressof.hpp"
#include "boost/core/lightweight_test.hpp"

using boost::optional;

std::string global("text"); 

optional<std::string&> make_optional_string_ref()
{
  return optional<std::string&>(global);
}

std::string& return_global()
{
  return global;
}

int main()
{
  optional<std::string&> opt;
  opt = make_optional_string_ref();
  BOOST_TEST(bool(opt));
  BOOST_TEST(*opt == global);
  BOOST_TEST(boost::addressof(*opt) == boost::addressof(global));
  
  {
    std::string& str = *make_optional_string_ref();
    BOOST_TEST(str == global);
    BOOST_TEST(boost::addressof(str) == boost::addressof(global));
  }
  
  {
    std::string& str = make_optional_string_ref().value();
    BOOST_TEST(str == global);
    BOOST_TEST(boost::addressof(str) == boost::addressof(global));
  }
  
  {
    std::string& str = make_optional_string_ref().value_or(global);
    BOOST_TEST(str == global);
    BOOST_TEST(boost::addressof(str) == boost::addressof(global));
  }
  
  {
    std::string& str = make_optional_string_ref().value_or_eval(&return_global);
    BOOST_TEST(str == global);
    BOOST_TEST(boost::addressof(str) == boost::addressof(global));
  }
  
  return boost::report_errors();
}
