//////////////////////////////////////////////////////////////////////////////
// Copyright 2005-2006 Andreas Huber Doenni
// Distributed under the Boost Software License, Version 1.0. (See accompany-
// ing file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////



#define BOOST_ENABLE_ASSERT_HANDLER

static int s_failed_assertions = 0;

namespace boost
{

void assertion_failed(
  char const *, char const *, char const *, long )
{
  ++s_failed_assertions;
}

} // namespace boost


#include <boost/statechart/result.hpp>
#include <boost/test/test_tools.hpp>


namespace sc = boost::statechart;


void make_unconsumed_result()
{
  // We cannot test sc::result in its natural environment here because a
  // failing assert triggers a stack unwind, what will lead to another
  // failing assert...

  // Creates a temp sc::result value which is destroyed immediately
  sc::detail::result_utility::make_result( sc::detail::do_discard_event );
}

int test_main( int, char* [] )
{
  make_unconsumed_result();

#ifdef NDEBUG
  BOOST_TEST( s_failed_assertions == 0 );
#else
  BOOST_TEST( s_failed_assertions == 1 );
#endif

  return 0;
}
