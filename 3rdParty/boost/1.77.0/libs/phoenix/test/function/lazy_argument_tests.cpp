////////////////////////////////////////////////////////////////////////////
// lazy_argument_tests.cpp
//
// lazy argument tests passing lazy function as argument.
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

namespace example {
      struct G {

            template <typename Sig>
            struct result;

            template <typename This, typename A0>
            struct result<This(A0)>
               : boost::remove_reference<A0>
            {};

            template <typename T>
            T operator()(T t) const { return ++t; }

      };
}

typedef boost::phoenix::function<example::G> GG;
boost::phoenix::function<example::G> gg;

template <typename F,typename T>
T h(F f, T const& t)
{
  return f(t)();
}

int main()
{
  BOOST_TEST( h(gg,1) == 2);
  BOOST_TEST(( h<GG,int>(gg,1) == 2));

  return boost::report_errors();
}
