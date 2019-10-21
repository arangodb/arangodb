////////////////////////////////////////////////////////////////////////////
// lazy_templated_struct_tests.cpp
//
// lazy templated struct test to check this works everywhere.
//
////////////////////////////////////////////////////////////////////////////
/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix/core/limits.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/function.hpp>
#include <boost/function.hpp>

namespace example {

  namespace impl {
    // Example of templated struct.
    template <typename Result>
    struct what {

      typedef Result result_type;

      Result operator()(Result const & r) const
      {
        return r;
      }
    };

    template <typename Result>
    struct what0 {

      typedef Result result_type;

      Result operator()() const
      {
        return Result(100);
      }

    };

  }

  boost::function1<int, int > what_int = impl::what<int>();
  boost::function0<int> what0_int = impl::what0<int>();
  BOOST_PHOENIX_ADAPT_FUNCTION(int,what,what_int,1)
  BOOST_PHOENIX_ADAPT_FUNCTION_NULLARY(int,what0,what0_int)
}

int main()
{
    int a = 99;
    using boost::phoenix::arg_names::arg1;
    BOOST_TEST(example::what_int(a) == a);
    BOOST_TEST(example::what(a)() == a);
    BOOST_TEST(example::what(arg1)(a) == a);
    BOOST_TEST(example::what0_int() == 100);
    BOOST_TEST(example::what0()() == 100);

    return boost::report_errors();
}
