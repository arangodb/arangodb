// Copyright Abel Sinkovics (abel@sinkovics.hu)  2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/foldl1.hpp>
#include <boost/metaparse/build_parser.hpp>
#include <boost/metaparse/transform.hpp>
#include <boost/metaparse/one_of_c.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/metaparse/util/digit_to_int.hpp>

#include <boost/mpl/int.hpp>
#include <boost/mpl/plus.hpp>
#include <boost/mpl/times.hpp>

#include <iostream>

using boost::metaparse::foldl1;
using boost::metaparse::build_parser;
using boost::metaparse::transform;
using boost::metaparse::one_of_c;
using boost::metaparse::entire_input;

using boost::metaparse::util::digit_to_int;

using boost::mpl::int_;
using boost::mpl::plus;
using boost::mpl::times;

/*
 * The grammar
 *
 * expression ::= ('1' | '0')*
 */

struct next_element
{
  template <class Acc, class B>
  struct apply : plus<times<Acc, int_<2> >, B> {};
};

typedef
  foldl1<transform<one_of_c<'0', '1'>, digit_to_int<> >, int_<0>, next_element>
  S;

typedef build_parser<entire_input<S> > binary_parser;

template <class S>
struct binary : binary_parser::apply<S>::type {};

#ifdef _STR
#  error _STR already defined
#endif
#define _STR BOOST_METAPARSE_STRING

#ifdef BOOST_NO_CXX11_CONSTEXPR

int main()
{
  using std::cout;
  using std::endl;
  using boost::metaparse::string;
  
  cout
    << binary<string<'1','0','0'> >::value << endl
    << binary<string<'1','0','1','1'> >::value << endl
    << binary<string<'1'> >::value << endl;
}
#else
int main()
{
  using std::cout;
  using std::endl;
  
  cout
    << binary<_STR("100")>::value << endl
    << binary<_STR("1011")>::value << endl
    << binary<_STR("1")>::value << endl;
}
#endif


