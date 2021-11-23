// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/grammar.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/string.hpp>

#include "test_case.hpp"

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/assert.hpp>

using boost::mpl::char_;

namespace
{
  struct next_char
  {
    typedef next_char type;

    template <class C>
    struct apply
    {
      typedef char_<C::type::value + 1> type;
    };
  };
}

BOOST_METAPARSE_TEST_CASE(grammar)
{
  using boost::metaparse::grammar;
  using boost::metaparse::lit_c;
  using boost::metaparse::get_result;
  using boost::metaparse::start;
  using boost::metaparse::is_error;
  using boost::metaparse::string;

  using boost::mpl::apply_wrap2;
  using boost::mpl::equal_to;
  using boost::mpl::equal;
  using boost::mpl::vector;

  // import
  BOOST_MPL_ASSERT((
        equal_to<
        char_<'x'>,
        get_result<
        apply_wrap2<
        grammar<>
        ::import<string<'S'>, lit_c<'x'> >::type,
          string<'x'>,
          start
        >
      >::type
    >
  ));

  // rename_import
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'x'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::import<string<'I'>, lit_c<'x'> >::type
            ::rule<string<'S',' ',':',':','=',' ','I'> >::type,
          string<'x'>,
          start
        >
      >::type
    >
  ));

  // char
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'x'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'S',' ',':',':','=',' ','\'','x','\''> >::type,
          string<'x'>,
          start
        >
      >::type
    >
  ));

  // char_failure
  BOOST_MPL_ASSERT((
    is_error<
      apply_wrap2<
        grammar<>
          ::rule<string<'S',' ',':',':','=',' ','\'','x','\''> >::type,
        string<'y'>,
        start
      >
    >
  ));

  // char_n
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'\n'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'S',' ',':',':','=',' ','\'','\\','n','\''> >::type,
          string<'\n'>,
          start
        >
      >::type
    >
  ));

  // char_r
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'\r'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'S',' ',':',':','=',' ','\'','\\','r','\''> >::type,
          string<'\r'>,
          start
        >
      >::type
    >
  ));

  // char_t
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'\t'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'S',' ',':',':','=',' ','\'','\\','t','\''> >::type,
          string<'\t'>,
          start
        >
      >::type
    >
  ));

  // backslash 
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'\\'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'S',' ',':',':','=',' ','\'','\\','\\','\''> >::type,
          string<'\\'>,
          start
        >
      >::type
    >
  ));

  // char_\'
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'\''>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'S',' ',':',':','=',' ','\'','\\','\'','\''> >::type,
          string<'\''>,
          start
        >
      >::type
    >
  ));

  // rename_rule
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'x'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'R',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','R'> >::type,
          string<'x'>,
          start
        >
      >::type
    >
  ));

  // sequence
  BOOST_MPL_ASSERT((
    equal<
      vector<char_<'x'>, char_<'x'> >,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'R',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','R',' ','R'> >::type,
          string<'x','x'>,
          start
        >
      >::type
    >
  ));

  // sequence_first_fail
  BOOST_MPL_ASSERT((
    is_error<
      apply_wrap2<
        grammar<>
          ::rule<string<'R',' ',':',':','=',' ','\'','x','\''> >::type
          ::rule<string<'S',' ',':',':','=',' ','R',' ','R'> >::type,
        string<'y','x'>,
        start
      >
    >
  ));

  // sequence_second_fail
  BOOST_MPL_ASSERT((
    is_error<
      apply_wrap2<
        grammar<>
          ::rule<string<'R',' ',':',':','=',' ','\'','x','\''> >::type
          ::rule<string<'S',' ',':',':','=',' ','R',' ','R'> >::type,
        string<'x','y'>,
        start
      >
    >
  ));

  // selection 1
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'x'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'Y',' ',':',':','=',' ','\'','y','\''> >::type
            ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','X','|','Y'> >::type,
          string<'x'>,
          start
        >
      >::type
    >
  ));

  // selection 2
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'y'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'Y',' ',':',':','=',' ','\'','y','\''> >::type
            ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','X','|','Y'> >::type,
          string<'y'>,
          start
        >
      >::type
    >
  ));

  // selection_fail
  BOOST_MPL_ASSERT((
    is_error<
      apply_wrap2<
        grammar<>
          ::rule<string<'Y',' ',':',':','=',' ','\'','y','\''> >::type
          ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
          ::rule<string<'S',' ',':',':','=',' ','X','|','Y'> >::type,
        string<'z'>,
        start
      >
    >
  ));

  // repeated_0
  BOOST_MPL_ASSERT((
    equal<
      vector<>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','X','*'> >::type,
          string<'y'>,
          start
        >
      >::type
    >
  ));

  // repeated_1
  BOOST_MPL_ASSERT((
    equal<
      vector<char_<'x'> >,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','X','*'> >::type,
          string<'x','y'>,
          start
        >
      >::type
    >
  ));

  // repeated_2
  BOOST_MPL_ASSERT((
    equal<
      vector<char_<'x'>, char_<'x'> >,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','X','*'> >::type,
          string<'x','x','y'>,
          start
        >
      >::type
    >
  ));

  // bracket
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'x'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<
              string<'S',' ',':',':','=',' ','(','\'','x','\'',')'>
            >::type,
          string<'x'>,
          start
        >
      >::type
    >
  ));

  // semantic_action
  BOOST_MPL_ASSERT((
    equal_to<
      char_<'y'>,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<
              string<'S',' ',':',':','=',' ','\'','x','\''>,
              next_char
            >::type,
          string<'x'>,
          start
        >
      >::type
    >
  ));

  // repeated+_0
  BOOST_MPL_ASSERT((
    is_error<
      apply_wrap2<
        grammar<>
          ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
          ::rule<string<'S',' ',':',':','=',' ','X','+'> >::type,
        string<'y'>,
        start
      >
    >
  ));

  // repeated+_1
  BOOST_MPL_ASSERT((
    equal<
      vector<char_<'x'> >,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','X','+'> >::type,
          string<'x','y'>,
          start
        >
      >::type
    >
  ));

  // repeated+_2
  BOOST_MPL_ASSERT((
    equal<
      vector<char_<'x'>, char_<'x'> >,
      get_result<
        apply_wrap2<
          grammar<>
            ::rule<string<'X',' ',':',':','=',' ','\'','x','\''> >::type
            ::rule<string<'S',' ',':',':','=',' ','X','+'> >::type,
          string<'x','x','y'>,
          start
        >
      >::type
    >
  ));
}


