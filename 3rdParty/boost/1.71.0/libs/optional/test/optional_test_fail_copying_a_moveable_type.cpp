// Copyright (C) 2014, Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com
//
#include "boost/optional.hpp"

class MoveOnly
{
public:
  int val;
  MoveOnly(int v) : val(v) {}
  MoveOnly(MoveOnly&& rhs) : val(rhs.val) { rhs.val = 0; }
  void operator=(MoveOnly&& rhs) {val = rhs.val; rhs.val = 0; }
  
private:
  MoveOnly(MoveOnly const&);
  void operator=(MoveOnly const&);
};

//
// THIS TEST SHOULD FAIL TO COMPILE
//
void test_copying_optional_with_noncopyable_T()
{
  boost::optional<MoveOnly> opt1 ;
  boost::optional<MoveOnly> opt2(opt1) ;
}


