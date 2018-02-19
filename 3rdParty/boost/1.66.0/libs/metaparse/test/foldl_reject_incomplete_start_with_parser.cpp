// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/foldl_reject_incomplete_start_with_parser.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/string.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/return_.hpp>
#include <boost/metaparse/sequence.hpp>
#include <boost/metaparse/v1/impl/back_inserter.hpp>

#include "common.hpp"

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

namespace
{
  struct keep_state
  {
    typedef keep_state type;

    template <class S, class C>
    struct apply : S {};
  };
}

BOOST_METAPARSE_TEST_CASE(foldl_reject_incomplete_start_with_parser)
{
  using boost::metaparse::foldl_reject_incomplete_start_with_parser;
  using boost::metaparse::start;
  using boost::metaparse::is_error;
  using boost::metaparse::lit_c;
  using boost::metaparse::get_result;
  using boost::metaparse::sequence;
  
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;
  using boost::mpl::char_;

  typedef sequence<lit_c<'a'>, lit_c<'a'> > aa;

  typedef
    foldl_reject_incomplete_start_with_parser<aa, lit_c<'b'>, keep_state>
    p;

  // test_b
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<p, str_b, start> >::type, char_<'b'> >
  ));
  
  // test_ba
  BOOST_MPL_ASSERT(( is_error<apply_wrap2<p, str_ba, start> > ));

  // test_baaaa
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<p, str_baaaa, start> >::type, char_<'b'> >
  ));

  // test_c
  BOOST_MPL_ASSERT((is_error<apply_wrap2<p, str_c, start> >));

  // test_ca
  BOOST_MPL_ASSERT((is_error<apply_wrap2<p, str_ca, start> >));
}

// Test foldl_reject_incomplete_start_with_parser as a normal fold

using boost::metaparse::foldl_reject_incomplete_start_with_parser;
using boost::metaparse::return_;
using boost::metaparse::v1::impl::back_inserter;

using boost::mpl::vector;

namespace
{
  template <class P>
  struct repeated_reject_incomplete :
    foldl_reject_incomplete_start_with_parser<
      P,
      return_<vector<> >,
      back_inserter
    >
  {};
}

#define TEST_NAME \
  foldl_reject_incomplete_start_with_parser_as_foldl_reject_incomplete

#include "repeated_reject_incomplete_test.hpp"

