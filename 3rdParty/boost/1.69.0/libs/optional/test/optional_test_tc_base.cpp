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

#if defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

int main()
{
}

#else

#include <utility>

struct NotDefaultConstructible
{
	NotDefaultConstructible() = delete;
};

void test_tc_base()
{
  boost::optional<NotDefaultConstructible> o;
  
  BOOST_TEST(boost::none == o);
}

struct S
{

};

template<class T>
struct W
{
	T& t_;

	template<class... Args>
	W(Args&&... args)
		: t_(std::forward<Args>(args)...)
	{
	}
};

void test_value_init()
{
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
	{
		S s;
		W<S> w{s};
	}
#endif
	{
		S s;
		W<S> w(s);
	}
}

void test_optoinal_reference_wrapper()
{
	boost::optional<W<S&> > o;
	BOOST_TEST(boost::none == o);
}

int main()
{
  test_tc_base();
  test_value_init();
  test_optoinal_reference_wrapper();
  return boost::report_errors();
}

#endif
