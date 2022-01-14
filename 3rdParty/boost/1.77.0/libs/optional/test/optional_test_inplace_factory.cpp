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

void test_ctor()
{
#ifndef BOOST_OPTIONAL_NO_INPLACE_FACTORY_SUPPORT
  Guard g0, g1(1.0, "one"), g2(2.0, "two");

  boost::optional<Guard> og0 ( boost::in_place() );
  boost::optional<Guard> og1 ( boost::in_place(1.0, "one") );
  boost::optional<Guard> og1_( boost::in_place(1.0, "one") );
  boost::optional<Guard> og2 ( boost::in_place<Guard>(2.0, "two") );

  BOOST_TEST(og0);
  BOOST_TEST(og1);
  BOOST_TEST(og1_);
  BOOST_TEST(og2);

  BOOST_TEST(*og0  == g0);
  BOOST_TEST(*og1  == g1);
  BOOST_TEST(*og1_ == g1);
  BOOST_TEST(*og2  == g2);

  BOOST_TEST(og1_ == og1);
  BOOST_TEST(og1_ != og2);
  BOOST_TEST(og1_ != og0);

  boost::optional<unsigned int> o( boost::in_place(5) );
  BOOST_TEST(o);
  BOOST_TEST(*o == 5);
#endif
}

void test_assign()
{
#ifndef BOOST_OPTIONAL_NO_INPLACE_FACTORY_SUPPORT
#ifndef BOOST_OPTIONAL_WEAK_OVERLOAD_RESOLUTION
  Guard g0, g1(1.0, "one"), g2(2.0, "two");

  boost::optional<Guard> og0, og1, og1_, og2;

  og0  = boost::in_place();
  og1  = boost::in_place(1.0, "one");
  og1_ = boost::in_place(1.0, "one");
  og2  = boost::in_place<Guard>(2.0, "two");

  BOOST_TEST(og0);
  BOOST_TEST(og1);
  BOOST_TEST(og1_);
  BOOST_TEST(og2);

  BOOST_TEST(*og0  == g0);
  BOOST_TEST(*og1  == g1);
  BOOST_TEST(*og1_ == g1);
  BOOST_TEST(*og2  == g2);

  BOOST_TEST(og1_ == og1);
  BOOST_TEST(og1_ != og2);
  BOOST_TEST(og1_ != og0);

  boost::optional<unsigned int> o;
  o = boost::in_place(5);
  BOOST_TEST(o);
  BOOST_TEST(*o == 5);
#endif
#endif
}

int main()
{
  test_ctor();
  test_assign();
  return boost::report_errors();
}
