// Copyright Abel Sinkovics (abel@sinkovics.hu) 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/reject.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_message.hpp>
#include <boost/metaparse/get_position.hpp>

#include <boost/mpl/assert.hpp>

#include <boost/type_traits.hpp>

#include "test_case.hpp"

namespace
{
  template <class T>
  struct returns
  {
    typedef T type;
  };

  template <class T>
  struct get_foo
  {
    typedef typename T::foo type;
  };
}

BOOST_METAPARSE_TEST_CASE(reject)
{
  using boost::metaparse::reject;
  using boost::metaparse::start;
  using boost::metaparse::get_message;
  using boost::metaparse::get_position;

  using boost::is_same;

  // test_reject_is_metaprogramming_value
  BOOST_MPL_ASSERT((is_same<reject<int, start>, reject<int, start>::type>));

  // test_reject_is_not_lazy
  BOOST_MPL_ASSERT((
    is_same<
      reject<get_foo<int>, start>,
      reject<get_foo<int>, returns<start> >::type
    >
  ));

  // test_get_message_of_reject
  BOOST_MPL_ASSERT((is_same<int, get_message<reject<int, start> >::type>));

  // test_get_position_of_reject
  BOOST_MPL_ASSERT((is_same<start, get_position<reject<int, start> >::type>));
}

