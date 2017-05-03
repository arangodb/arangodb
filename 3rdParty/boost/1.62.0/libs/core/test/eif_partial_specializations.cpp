// Boost enable_if library

// Copyright 2003 (c) The Trustees of Indiana University.

// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//    Authors: Jaakko Jarvi (jajarvi at osl.iu.edu)
//             Jeremiah Willcock (jewillco at osl.iu.edu)
//             Andrew Lumsdaine (lums at osl.iu.edu)

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <boost/detail/lightweight_test.hpp>

using boost::enable_if_has_type;
using boost::enable_if_c;
using boost::disable_if_c;
using boost::enable_if;
using boost::disable_if;
using boost::is_arithmetic;

template <class T, class Enable = void>
struct tester;

template <class T>
struct tester<T, typename enable_if_c<is_arithmetic<T>::value>::type> {
  BOOST_STATIC_CONSTANT(bool, value = true);
};

template <class T>
struct tester<T, typename disable_if_c<is_arithmetic<T>::value>::type> {
  BOOST_STATIC_CONSTANT(bool, value = false);
};

template <class T, class Enable = void>
struct tester2;

template <class T>
struct tester2<T, typename enable_if<is_arithmetic<T> >::type> {
  BOOST_STATIC_CONSTANT(bool, value = true);
};

template <class T>
struct tester2<T, typename disable_if<is_arithmetic<T> >::type> {
  BOOST_STATIC_CONSTANT(bool, value = false);
};

template <class T, class Enable = void>
struct tester3
{
  typedef T type;
  BOOST_STATIC_CONSTANT(bool, value = false);
};

template <class T>
struct tester3<T, typename enable_if_has_type<typename T::value_type>::type>
{
  typedef typename T::value_type type;
  BOOST_STATIC_CONSTANT(bool, value = true);
};

struct sample_value_type
{
  typedef float***& value_type;
};

int main()
{

  BOOST_TEST(tester<int>::value);
  BOOST_TEST(tester<double>::value);

  BOOST_TEST(!tester<char*>::value);
  BOOST_TEST(!tester<void*>::value);

  BOOST_TEST(tester2<int>::value);
  BOOST_TEST(tester2<double>::value);

  BOOST_TEST(!tester2<char*>::value);
  BOOST_TEST(!tester2<void*>::value);

  BOOST_TEST(!tester3<char*>::value);
  BOOST_TEST(tester3<sample_value_type>::value);

  return boost::report_errors();
}

