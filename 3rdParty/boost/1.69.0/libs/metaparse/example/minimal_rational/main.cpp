// Copyright Abel Sinkovics (abel@sinkovics.hu)  2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/string.hpp>
#include <boost/metaparse/sequence_apply.hpp>
#include <boost/metaparse/last_of.hpp>
#include <boost/metaparse/int_.hpp>
#include <boost/metaparse/token.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/build_parser.hpp>

#include <boost/rational.hpp>

#include <boost/config.hpp>

#include <iostream>

using boost::metaparse::sequence_apply2;
using boost::metaparse::last_of;
using boost::metaparse::int_;
using boost::metaparse::token;
using boost::metaparse::lit_c;
using boost::metaparse::entire_input;
using boost::metaparse::build_parser;

template <class Num, class Denom>
struct rational
{
  typedef rational type;

  static boost::rational<int> run()
  {
    return boost::rational<int>(Num::type::value, Denom::type::value);
  }
};

typedef
  sequence_apply2<
    rational,

    token<int_>,
    last_of<lit_c<'/'>, token<int_> >
  >
  rational_grammar;

typedef build_parser<entire_input<rational_grammar> > rational_parser;

#ifdef RATIONAL
#  error RATIONAL already defined
#endif
#define RATIONAL(s) \
  (::rational_parser::apply<BOOST_METAPARSE_STRING(s)>::type::run())

#if BOOST_METAPARSE_STD < 2011

int main()
{
  std::cout << "Please use a compiler that supports constexpr" << std::endl;
}

#else

int main()
{
  const boost::rational<int> r1 = RATIONAL("1/3");
  const boost::rational<int> r2 = RATIONAL("4/4");
  const boost::rational<int> r3 = RATIONAL("13/11");

  std::cout
    << r1 << std::endl
    << r2 << std::endl
    << r3 << std::endl;
}

#endif

