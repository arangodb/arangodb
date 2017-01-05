// Boost enable_if library

// Copyright 2003 (c) The Trustees of Indiana University.

// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//    Authors: Jaakko Jarvi (jajarvi at osl.iu.edu)
//             Jeremiah Willcock (jewillco at osl.iu.edu)
//             Andrew Lumsdaine (lums at osl.iu.edu)

#include <boost/config.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/detail/lightweight_test.hpp>

using boost::enable_if;
using boost::is_arithmetic;

template<class T> struct not_
{
  BOOST_STATIC_CONSTANT( bool, value = !T::value );
};

template<class T>
typename enable_if<is_arithmetic<T>, bool>::type
arithmetic_object(T t) { return true; }

template<class T>
typename enable_if<not_<is_arithmetic<T> >, bool>::type
arithmetic_object(T t) { return false; }


int main()
{
 
  BOOST_TEST(arithmetic_object(1));
  BOOST_TEST(arithmetic_object(1.0));

  BOOST_TEST(!arithmetic_object("1"));
  BOOST_TEST(!arithmetic_object(static_cast<void*>(0)));

  return boost::report_errors();
}

