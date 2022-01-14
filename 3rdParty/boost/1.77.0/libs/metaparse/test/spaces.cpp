// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/spaces.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_remaining.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/not.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

namespace
{
  using boost::mpl::list_c;

  typedef list_c<char, 'e', 'l', 'l', 'o'> str_ello;
  typedef
    list_c<char, ' ', '\t', '\n', '\r', 'e', 'l', 'l', 'o'>
    str_____ello;

}

BOOST_METAPARSE_TEST_CASE(spaces)
{
  using boost::metaparse::is_error;
  using boost::metaparse::spaces;
  using boost::metaparse::start;
  using boost::metaparse::get_remaining;
  
  using boost::mpl::apply_wrap2;
  using boost::mpl::not_;
  using boost::mpl::equal;

  // test_reject_no_space
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<spaces, str_hello, start> >
  ));

  // test_accept_one_space
  BOOST_MPL_ASSERT((
    not_<is_error<apply_wrap2<spaces, str__ello, start> > >
  ));

  // test_accept_only_space
  BOOST_MPL_ASSERT((
    equal<get_remaining<apply_wrap2<spaces, str__ello, start> >::type, str_ello>
  ));

  // test_accept_all_spaces
  BOOST_MPL_ASSERT((not_<is_error<apply_wrap2<spaces, str_____ello,start> > >));

  // test_consume_all_spaces
  BOOST_MPL_ASSERT((
    equal<
      get_remaining<apply_wrap2<spaces, str_____ello, start> >::type,
      str_ello
    >
  ));
}


