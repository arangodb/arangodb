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

#include "boost/core/lightweight_test.hpp"

using boost::optional;


// testing types:
// X is convertible to Y
// ADeriv is convertible to ABase
struct X
{
  int val;
  explicit X(int v) : val(v) {}
};

struct Y
{
  int yval;
  Y(X const& x) : yval(x.val) {}
  friend bool operator==(Y const& l, Y const& r) { return l.yval == r.yval; }
};

struct ABase
{
  int val;
  explicit ABase(int v) : val(v) {}
  friend bool operator==(ABase const& l, ABase const& r) { return l.val == r.val; }
};

struct ADeriv : ABase
{
    explicit ADeriv(int v) : ABase(v) {}
};


template <typename T, typename U>
void test_convert_optional_U_to_optional_T_for()
{
#ifndef BOOST_OPTIONAL_NO_CONVERTING_COPY_CTOR
  {
    optional<U> ou(U(8));
    optional<T> ot1(ou);
    BOOST_TEST(ot1);
    BOOST_TEST(*ot1 == T(*ou));
  }
#endif

#ifndef BOOST_OPTIONAL_NO_CONVERTING_ASSIGNMENT
  {
    optional<U> ou(U(8));
    optional<T> ot2;
    ot2 = ou;
    BOOST_TEST(ot2);
    BOOST_TEST(*ot2 == T(*ou));
  }
#endif
}

void test_convert_optional_U_to_optional_T()
{
  test_convert_optional_U_to_optional_T_for<Y, X>();
  test_convert_optional_U_to_optional_T_for<ABase, ADeriv>();
  test_convert_optional_U_to_optional_T_for<long, short>();
  test_convert_optional_U_to_optional_T_for<double, float>();
}

template <typename T, typename U>
void test_convert_U_to_optional_T_for()
{
  U u(8);
  optional<T> ot1(u);
  BOOST_TEST(ot1);
  BOOST_TEST(*ot1 == T(u));
  
  optional<T> ot2;
  ot2 = u;
  BOOST_TEST(ot2);
  BOOST_TEST(*ot2 == T(u));
}

void test_convert_U_to_optional_T()
{
  test_convert_U_to_optional_T_for<Y, X>();
  test_convert_U_to_optional_T_for<ABase, ADeriv>();
  test_convert_U_to_optional_T_for<long, short>();
  test_convert_U_to_optional_T_for<double, float>();
}

int main()
{
  test_convert_optional_U_to_optional_T();
  test_convert_U_to_optional_T();
  
  return boost::report_errors();
}
