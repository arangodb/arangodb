// Copyright Abel Sinkovics (abel@sinkovics.hu)  2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/repeated.hpp>
#include <boost/metaparse/sequence.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/last_of.hpp>
#include <boost/metaparse/first_of.hpp>
#include <boost/metaparse/middle_of.hpp>
#include <boost/metaparse/space.hpp>
#include <boost/metaparse/int_.hpp>
#include <boost/metaparse/foldl_reject_incomplete_start_with_parser.hpp>
#include <boost/metaparse/foldr_start_with_parser.hpp>
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
#include <boost/mpl/negate.hpp>
#include <boost/mpl/char.hpp>

using boost::metaparse::sequence;
using boost::metaparse::lit_c;
using boost::metaparse::last_of;
using boost::metaparse::first_of;
using boost::metaparse::middle_of;
using boost::metaparse::space;
using boost::metaparse::repeated;
using boost::metaparse::build_parser;
using boost::metaparse::int_;
using boost::metaparse::foldl_reject_incomplete_start_with_parser;
using boost::metaparse::foldr_start_with_parser;
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
using boost::mpl::negate;
using boost::mpl::char_;

/*
 * The grammar
 *
 * expression ::= plus_exp
 * plus_exp ::= prod_exp ((plus_token | minus_token) prod_exp)*
 * prod_exp ::= int_token ((mult_token | div_token) simple_exp)*
 * simple_exp ::= (plus_token | minus_token)* (int_token | paren_exp)
 * paren_exp ::= open_paren_token expression close_paren_token
 */

typedef token<lit_c<'+'> > plus_token;
typedef token<lit_c<'-'> > minus_token;
typedef token<lit_c<'*'> > mult_token;
typedef token<lit_c<'/'> > div_token;

typedef token<lit_c<'('> > open_paren_token;
typedef token<lit_c<')'> > close_paren_token;
 
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

struct eval_unary_plus
{
  template <class State, class C>
  struct apply :
    eval_if<
      typename equal_to<char_<'+'>, typename C::type>::type,
      State, // +State is State
      negate<typename State::type>
    >
  {};
};

struct plus_exp;

typedef middle_of<open_paren_token, plus_exp, close_paren_token> paren_exp;

typedef
  foldr_start_with_parser<
    one_of<plus_token, minus_token>,
    one_of<int_token, paren_exp>,
    eval_unary_plus
  >
  simple_exp;

typedef
  foldl_reject_incomplete_start_with_parser<
    sequence<one_of<mult_token, div_token>, simple_exp>,
    simple_exp,
    eval_mult
  >
  prod_exp;
  
struct plus_exp :
  foldl_reject_incomplete_start_with_parser<
    sequence<one_of<plus_token, minus_token>, prod_exp>,
    prod_exp,
    eval_plus
  >
{};

typedef last_of<repeated<space>, plus_exp> expression;

typedef build_parser<entire_input<expression> > calculator_parser;

#ifdef _STR
#  error _STR already defined
#endif
#define _STR BOOST_METAPARSE_STRING

#ifdef BOOST_NO_CXX11_CONSTEXPR
int main()
{
  std::cout << "Please use a compiler that support constexpr" << std::endl;
}
#else
int main()
{
  using std::cout;
  using std::endl;
  
  cout
    << apply_wrap1<calculator_parser, _STR("13")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR(" 1+ 2*4-6/2")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("1+2*3+4")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("(1+2)*(3+4)")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("(2*3)+(4*5)")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("2 * 3 + 4")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("2 + (3 * 4)")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("+3")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("-21")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("24 + - 21")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("- - 21")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("-(3 * 4)")>::type::value << endl
    ;
}
#endif

