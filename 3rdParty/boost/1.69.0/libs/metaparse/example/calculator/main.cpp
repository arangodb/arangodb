// Copyright Abel Sinkovics (abel@sinkovics.hu)  2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/repeated.hpp>
#include <boost/metaparse/sequence.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/last_of.hpp>
#include <boost/metaparse/first_of.hpp>
#include <boost/metaparse/space.hpp>
#include <boost/metaparse/int_.hpp>
#include <boost/metaparse/foldl_reject_incomplete_start_with_parser.hpp>
#include <boost/metaparse/one_of.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/token.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/string.hpp>
#include <boost/metaparse/build_parser.hpp>

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/plus.hpp>
#include <boost/mpl/minus.hpp>
#include <boost/mpl/times.hpp>
#include <boost/mpl/divides.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/bool.hpp>

using boost::metaparse::sequence;
using boost::metaparse::lit_c;
using boost::metaparse::last_of;
using boost::metaparse::first_of;
using boost::metaparse::space;
using boost::metaparse::repeated;
using boost::metaparse::build_parser;
using boost::metaparse::int_;
using boost::metaparse::foldl_reject_incomplete_start_with_parser;
using boost::metaparse::get_result;
using boost::metaparse::one_of;
using boost::metaparse::token;
using boost::metaparse::entire_input;

using boost::mpl::apply_wrap1;
using boost::mpl::fold;
using boost::mpl::front;
using boost::mpl::back;
using boost::mpl::plus;
using boost::mpl::minus;
using boost::mpl::times;
using boost::mpl::divides;
using boost::mpl::eval_if;
using boost::mpl::bool_;
using boost::mpl::equal_to;
using boost::mpl::bool_;

/*
 * The grammar
 *
 * expression ::= plus_exp
 * plus_exp ::= prod_exp ((plus_token | minus_token) prod_exp)*
 * prod_exp ::= int_token ((mult_token | div_token) int_token)*
 */

typedef token<lit_c<'+'> > plus_token;
typedef token<lit_c<'-'> > minus_token;
typedef token<lit_c<'*'> > mult_token;
typedef token<lit_c<'/'> > div_token;
 
typedef token<int_> int_token;

template <class T, char C>
struct is_c : bool_<T::type::value == C> {};

struct eval_plus
{
  template <class State, class C>
  struct apply :
    eval_if<
      is_c<front<C>, '+'>,
      plus<typename State::type, typename back<C>::type>,
      minus<typename State::type, typename back<C>::type>
    >
  {};
};

struct eval_mult
{
  template <class State, class C>
  struct apply :
    eval_if<
      is_c<front<C>, '*'>,
      times<typename State::type, typename back<C>::type>,
      divides<typename State::type, typename back<C>::type>
    >
  {};
};

typedef
  foldl_reject_incomplete_start_with_parser<
    sequence<one_of<mult_token, div_token>, int_token>,
    int_token,
    eval_mult
  >
  prod_exp;
  
typedef
  foldl_reject_incomplete_start_with_parser<
    sequence<one_of<plus_token, minus_token>, prod_exp>,
    prod_exp,
    eval_plus
  >
  plus_exp;

typedef last_of<repeated<space>, plus_exp> expression;

typedef build_parser<entire_input<expression> > calculator_parser;

#ifdef _STR
#  error _STR already defined
#endif
#define _STR BOOST_METAPARSE_STRING

#if BOOST_METAPARSE_STD < 2011
int main()
{
  using std::cout;
  using std::endl;
  using boost::metaparse::string;
  
  cout
    << apply_wrap1<calculator_parser, string<'1','3'> >::type::value << endl
    <<
      apply_wrap1<
        calculator_parser, string<' ','1','+',' ','2','*','4','-','6','/','2'>
      >::type::value
    << endl
    ;
}
#else
int main()
{
  using std::cout;
  using std::endl;
  
  cout
    << apply_wrap1<calculator_parser, _STR("13")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR(" 1+ 2*4-6/2")>::type::value << endl
    ;
}
#endif


