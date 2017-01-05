// Copyright Abel Sinkovics (abel@sinkovics.hu)  2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#ifdef BOOST_NO_CXX11_CONSTEXPR
#include <iostream>

int main()
{
  std::cout << "Please use a compiler that supports constexpr" << std::endl;
}
#else

#define BOOST_MPL_LIMIT_STRING_SIZE 64 
#define BOOST_METAPARSE_LIMIT_STRING_SIZE BOOST_MPL_LIMIT_STRING_SIZE

#include <boost/metaparse/grammar.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/build_parser.hpp>
#include <boost/metaparse/token.hpp>
#include <boost/metaparse/string.hpp>
#include <boost/metaparse/util/digit_to_int.hpp>

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/plus.hpp>
#include <boost/mpl/minus.hpp>
#include <boost/mpl/times.hpp>
#include <boost/mpl/divides.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/lambda.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/int.hpp>

using boost::metaparse::build_parser;
using boost::metaparse::entire_input;
using boost::metaparse::token;
using boost::metaparse::grammar;

using boost::metaparse::util::digit_to_int;

using boost::mpl::apply_wrap1;
using boost::mpl::fold;
using boost::mpl::front;
using boost::mpl::back;
using boost::mpl::plus;
using boost::mpl::minus;
using boost::mpl::times;
using boost::mpl::divides;
using boost::mpl::eval_if;
using boost::mpl::equal_to;
using boost::mpl::_1;
using boost::mpl::_2;
using boost::mpl::char_;
using boost::mpl::lambda;
using boost::mpl::int_;

#ifdef _STR
  #error _STR already defined
#endif
#define _STR BOOST_METAPARSE_STRING

template <class A, class B>
struct lazy_plus : plus<typename A::type, typename B::type> {};

template <class A, class B>
struct lazy_minus : minus<typename A::type, typename B::type> {};

template <class A, class B>
struct lazy_times : times<typename A::type, typename B::type> {};

template <class A, class B>
struct lazy_divides : divides<typename A::type, typename B::type> {};

template <class C, class T, class F>
struct lazy_eval_if : eval_if<typename C::type, T, F> {};

template <class A, class B>
struct lazy_equal_to : equal_to<typename A::type, typename B::type> {};

template <class Sequence, class State, class ForwardOp>
struct lazy_fold :
  fold<typename Sequence::type, typename State::type, typename ForwardOp::type>
{};

typedef
  lazy_fold<
    back<_1>,
    front<_1>,
    lambda<
      lazy_eval_if<
        lazy_equal_to<front<_2>, char_<'*'>>,
        lazy_times<_1, back<_2>>,
        lazy_divides<_1, back<_2>>
      >
    >::type
  >
  prod_action;

typedef
  lazy_fold<
    back<_1>,
    front<_1>,
    lambda<
      lazy_eval_if<
        lazy_equal_to<front<_2>, char_<'+'>>,
        lazy_plus<_1, back<_2>>,
        lazy_minus<_1, back<_2>>
      >
    >::type
  >
  plus_action;

typedef
  lambda<
    lazy_fold<
      _1,
      int_<0>,
      lambda<
        lazy_plus<lazy_times<_1, int_<10>>, apply_wrap1<digit_to_int<>, _2>>
      >::type
    >
  >::type
  int_action;

typedef
  grammar<_STR("plus_exp")>

    ::rule<_STR("int ::= ('0'|'1'|'2'|'3'|'4'|'5'|'6'|'7'|'8'|'9')+"), int_action>::type
    ::rule<_STR("ws ::= (' ' | '\n' | '\r' | '\t')*")>::type
    ::rule<_STR("int_token ::= int ws"), front<_1>>::type
    ::rule<_STR("plus_token ::= '+' ws"), front<_1>>::type
    ::rule<_STR("minus_token ::= '-' ws"), front<_1>>::type
    ::rule<_STR("mult_token ::= '*' ws"), front<_1>>::type
    ::rule<_STR("div_token ::= '/' ws"), front<_1>>::type
    ::rule<_STR("plus_token ::= '+' ws")>::type
    ::rule<_STR("plus_exp ::= prod_exp ((plus_token | minus_token) prod_exp)*"), plus_action>::type
    ::rule<_STR("prod_exp ::= int_token ((mult_token | div_token) int_token)*"), prod_action>::type
  expression;

typedef build_parser<entire_input<expression>> calculator_parser;

int main()
{
  using std::cout;
  using std::endl;
  
  cout
    << apply_wrap1<calculator_parser, _STR("13")>::type::value << endl
    << apply_wrap1<calculator_parser, _STR("1+ 2*4-6/2")>::type::value << endl
    ;
}
#endif

