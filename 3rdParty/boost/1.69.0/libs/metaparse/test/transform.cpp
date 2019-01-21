// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/transform.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/repeated.hpp>
#include <boost/metaparse/one_char.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/always.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

namespace
{
  using boost::metaparse::repeated;
  using boost::metaparse::one_char;

  using boost::mpl::always;

  typedef always<char_x> f;
  typedef repeated<one_char> repeated_one_char;

  struct get_front
  {
    typedef get_front type;

    template <class C>
    struct apply : boost::mpl::front<C> {};
  };
}

BOOST_METAPARSE_TEST_CASE(transform)
{
  using boost::metaparse::get_result;
  using boost::metaparse::transform;
  using boost::metaparse::start;
  using boost::metaparse::is_error;
  
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;

  // test_normal_case
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<transform<lit_h, f>, str_hello, start> >::type,
      char_x
    >
  ));

  // test_parser_fails
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<transform<lit_x, f>, str_hello, start> >
  ));

  // test_empty_input
  BOOST_MPL_ASSERT((is_error<apply_wrap2<transform<lit_h, f>, str_, start> >));
  
  // test_tranformation_functions_arg
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<
        apply_wrap2<transform<repeated_one_char, get_front>, str_hello, start>
      >::type,
      char_h
    >
  ));
}


