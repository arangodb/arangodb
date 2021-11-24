// Copyright (C) 2003, Fernando Luis Cacciola Carballal.
// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  fernando_cacciola@hotmail.com

#include <sstream>
#include "boost/optional/optional.hpp"
#include "boost/optional/optional_io.hpp"

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"

using boost::optional;

template<class Opt>
void test2( Opt o, Opt buff )
{
  std::stringstream s ;

  const int markv = 123 ;
  int mark = 0 ;
  
  s << o << " " << markv ;
  s >> buff >> mark ;

  BOOST_TEST( buff == o ) ;
  BOOST_TEST( mark == markv ) ;
}


template<class T>
void test( T v, T w )
{
  test2( boost::make_optional(v), optional<T>  ());
  test2( boost::make_optional(v), boost::make_optional(w));
  test2( optional<T>  () , optional<T>  ());
  test2( optional<T>  () , boost::make_optional(w));
}


template <class T>
void subtest_tag_none_reversibility_with_optional(optional<T> ov)
{
  std::stringstream s;
  s << boost::none;
  s >> ov;
  BOOST_TEST(!ov);
}

template <class T>
void subtest_tag_none_equivalence_with_optional()
{
  std::stringstream s, r;
  optional<T> ov;
  s << boost::none;
  r << ov;
  BOOST_TEST_EQ(s.str(), r.str());
}

template <class T>
void test_tag_none(T v)
{
  subtest_tag_none_reversibility_with_optional(optional<T>(v));
  subtest_tag_none_reversibility_with_optional(optional<T>());
  subtest_tag_none_equivalence_with_optional<T>();
}


int main()
{
  test(1,2);
  test(std::string("hello"), std::string("buffer"));
  test_tag_none(10);
  test_tag_none(std::string("text"));

  return boost::report_errors();
}
