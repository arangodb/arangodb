// Copyright (C) 2014-2015 Andrzej Krzemienski.
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

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#include <string>
#include "boost/core/lightweight_test.hpp"
#include "boost/none.hpp"

class NonConstructible
{
private:
    NonConstructible();
    NonConstructible(NonConstructible const&);
#if (!defined BOOST_NO_CXX11_RVALUE_REFERENCES)
    NonConstructible(NonConstructible &&);
#endif   
};

void test_non_constructible()
{
  boost::optional<NonConstructible> o;
  BOOST_TEST(!o);
  BOOST_TEST(o == boost::none);
  BOOST_TEST_THROWS(o.value(), boost::bad_optional_access);
}

class Guard
{
public:
    explicit Guard(int) {}
private:
    Guard();
    Guard(Guard const&);
#if (!defined BOOST_NO_CXX11_RVALUE_REFERENCES)
    Guard(Guard &&);
#endif   
};

void test_guard()
{
    boost::optional<Guard> o;
    o.emplace(1);
    BOOST_TEST(o);
    BOOST_TEST(o != boost::none);
}

void test_non_assignable()
{
    boost::optional<const std::string> o;
    o.emplace("cat");
    BOOST_TEST(o);
    BOOST_TEST(o != boost::none);
    BOOST_TEST_EQ(*o, std::string("cat"));
}

int main()
{
  test_non_constructible();
  test_guard();
  test_non_assignable();

  return boost::report_errors();
}