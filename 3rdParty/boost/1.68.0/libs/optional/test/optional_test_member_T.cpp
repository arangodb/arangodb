// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional/optional.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"

struct Status
{
  enum T
  {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
  };

  T mValue;
};

void test_member_T()
{
  boost::optional<Status> x = Status();
  x->mValue = Status::CONNECTED;
  
  BOOST_TEST(x->mValue == Status::CONNECTED);
}

int main()
{
  test_member_T();
  return boost::report_errors();
}
