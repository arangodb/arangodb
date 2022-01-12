// Copyright (C) 2003, Fernando Luis Cacciola Carballal.
// Copyright (C) 2015 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  fernando_cacciola@hotmail.com

#include<string>
#include "boost/optional/optional.hpp"

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#ifndef BOOST_OPTIONAL_NO_INPLACE_FACTORY_SUPPORT
#include "boost/utility/in_place_factory.hpp"
#include "boost/utility/typed_in_place_factory.hpp"
#endif

#include "boost/core/lightweight_test.hpp"
#include "boost/none.hpp"

struct Guard
{
  double num;
  std::string str;
  Guard() : num() {}
  Guard(double num_, std::string str_) : num(num_), str(str_) {}
  
  friend bool operator==(const Guard& lhs, const Guard& rhs) { return lhs.num == rhs.num && lhs.str == rhs.str; }
  friend bool operator!=(const Guard& lhs, const Guard& rhs) { return !(lhs == rhs); }
  
private:
  Guard(const Guard&);
  Guard& operator=(const Guard&);
};


int main()
{
#ifndef BOOST_OPTIONAL_NO_INPLACE_FACTORY_SUPPORT
  int excessive_param = 2;
  boost::optional<Guard> og1 ( boost::in_place(1.0, "one", excessive_param) );
#else
  NOTHING_TO_TEST_SO_JUST_FAIL
#endif
  return 0;
}