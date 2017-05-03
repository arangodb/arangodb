// Copyright Abel Sinkovics (abel@sinkovics.hu) 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE string

#define BOOST_METAPARSE_LIMIT_STRING_SIZE 64

#include <boost/metaparse/string.hpp>

#include <boost/mpl/equal.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/begin_end.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/advance.hpp>
#include <boost/mpl/next.hpp>
#include <boost/mpl/prior.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/clear.hpp>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/pop_back.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/push_back.hpp>
#include <boost/mpl/string.hpp>
#include <boost/mpl/assert.hpp>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_CASE(test_string)
{
  using boost::mpl::equal;
  using boost::mpl::equal_to;
  using boost::mpl::char_;
  using boost::mpl::int_;
  using boost::mpl::at;
  using boost::mpl::at_c;
  using boost::mpl::size;
  using boost::mpl::back;
  using boost::mpl::deref;
  using boost::mpl::advance;
  using boost::mpl::next;
  using boost::mpl::prior;
  using boost::mpl::distance;
  using boost::mpl::clear;
  using boost::mpl::empty;
  using boost::mpl::front;
  using boost::mpl::is_sequence;
  using boost::mpl::pop_front;
  using boost::mpl::pop_back;
  using boost::mpl::begin;
  using boost::mpl::end;
  using boost::mpl::c_str;
  using boost::mpl::push_front;
  using boost::mpl::push_back;

  using namespace boost;

  // string type
  ///////////////////////

  typedef metaparse::string<'H','e','l','l','o'> hello;
  typedef metaparse::string<> empty_string;

  typedef begin<hello>::type begin_hello;
  typedef end<hello>::type end_hello;

  // test_value_of_empty_string
  BOOST_REQUIRE_EQUAL(std::string(), c_str<empty_string>::type::value);

  // test_value
  BOOST_REQUIRE_EQUAL(std::string("Hello"), c_str<hello>::type::value);

  // equal_to
  BOOST_MPL_ASSERT((equal_to<hello, hello>));
  BOOST_MPL_ASSERT_NOT((equal_to<hello, empty_string>));

  // at
  BOOST_MPL_ASSERT((equal_to<char_<'e'>, at<hello, int_<1> >::type>));
  BOOST_MPL_ASSERT((equal_to<char_<'e'>, at_c<hello, 1>::type>));

  // size
  BOOST_MPL_ASSERT((equal_to<int_<5>, size<hello>::type>));

  // is_sequence
  BOOST_MPL_ASSERT((is_sequence<hello>));

  // front
  BOOST_MPL_ASSERT((equal_to<char_<'H'>, front<hello>::type>));

  // push_front
  BOOST_MPL_ASSERT((
    equal_to<
      metaparse::string<'x','H','e','l','l','o'>,
      push_front<hello, char_<'x'> >::type
    >
  ));

  // back
  BOOST_MPL_ASSERT((equal_to<char_<'o'>, back<hello>::type>));

  // push_back
  BOOST_MPL_ASSERT((
    equal_to<
      metaparse::string<'H','e','l','l','o', 'x'>,
      push_back<hello, char_<'x'> >::type
    >
  ));

  // clear
  BOOST_MPL_ASSERT((equal_to<empty_string, clear<hello>::type>));

  // empty
  BOOST_MPL_ASSERT_NOT((empty<hello>::type));
  BOOST_MPL_ASSERT((empty<empty_string>::type));

  // string_iterator
  BOOST_MPL_ASSERT((equal_to<begin_hello, begin_hello>));
  BOOST_MPL_ASSERT_NOT((equal_to<begin_hello, end_hello>));
  BOOST_MPL_ASSERT((
    equal_to<begin<empty_string>::type, end<empty_string>::type>
  ));

  BOOST_MPL_ASSERT((equal_to<char_<'H'>, deref<begin_hello>::type>));
  BOOST_MPL_ASSERT((equal_to<end_hello, advance<begin_hello, int_<5> >::type>));
  BOOST_MPL_ASSERT((
    equal_to<char_<'e'>, deref<next<begin_hello>::type>::type>
  ));
  BOOST_MPL_ASSERT((equal_to<char_<'o'>, deref<prior<end_hello>::type>::type>));

  BOOST_MPL_ASSERT((equal_to<int_<5>, distance<begin_hello, end_hello>::type>));

  // pop_front
  BOOST_MPL_ASSERT((
    equal_to<metaparse::string<'e','l','l','o'>, pop_front<hello>::type>
  ));

  // pop_back
  BOOST_MPL_ASSERT((
    equal_to<metaparse::string<'H','e','l','l'>, pop_back<hello>::type>
  ));

#ifndef BOOST_NO_CONSTEXPR
  // BOOST_METAPARSE_STRING macro
  ///////////////////////

  // test_empty_string
  BOOST_MPL_ASSERT((equal<BOOST_METAPARSE_STRING(""), empty_string>));

  // test_string_creation
  BOOST_MPL_ASSERT((equal<BOOST_METAPARSE_STRING("Hello"), hello>));
#endif

}

