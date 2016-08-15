// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/repeated.hpp>
#include <boost/metaparse/sequence.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/last_of.hpp>
#include <boost/metaparse/space.hpp>
#include <boost/metaparse/int_.hpp>
#include <boost/metaparse/foldl_reject_incomplete_start_with_parser.hpp>
#include <boost/metaparse/one_of.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/token.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/string.hpp>
#include <boost/metaparse/transform.hpp>
#include <boost/metaparse/always.hpp>
#include <boost/metaparse/build_parser.hpp>

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>

using boost::metaparse::sequence;
using boost::metaparse::lit_c;
using boost::metaparse::last_of;
using boost::metaparse::space;
using boost::metaparse::repeated;
using boost::metaparse::build_parser;
using boost::metaparse::int_;
using boost::metaparse::foldl_reject_incomplete_start_with_parser;
using boost::metaparse::get_result;
using boost::metaparse::one_of;
using boost::metaparse::token;
using boost::metaparse::entire_input;
using boost::metaparse::transform;
using boost::metaparse::always;

using boost::mpl::apply_wrap1;
using boost::mpl::front;
using boost::mpl::back;
using boost::mpl::if_;
using boost::mpl::bool_;

/*
 * The grammar
 *
 * expression ::= plus_exp
 * plus_exp ::= prod_exp ((plus_token | minus_token) prod_exp)*
 * prod_exp ::= value_exp ((mult_token | div_token) value_exp)*
 * value_exp ::= int_token | '_'
 */

typedef token<lit_c<'+'> > plus_token;
typedef token<lit_c<'-'> > minus_token;
typedef token<lit_c<'*'> > mult_token;
typedef token<lit_c<'/'> > div_token;
 
typedef token<int_> int_token;
typedef token<lit_c<'_'> > arg_token;

template <class T, char C>
struct is_c : bool_<T::type::value == C> {};

struct build_plus
{
  template <class A, class B>
  class _plus
  {
  public:
    typedef _plus type;

    template <class T>
    T operator()(T t) const
    {
      return _left(t) + _right(t);
    }
  private:
    typename A::type _left;
    typename B::type _right;
  };

  template <class A, class B>
  class _minus
  {
  public:
    typedef _minus type;

    template <class T>
    T operator()(T t) const
    {
      return _left(t) - _right(t);
    }
  private:
    typename A::type _left;
    typename B::type _right;
  };

  template <class State, class C>
  struct apply :
    if_<
      typename is_c<front<C>, '+'>::type,
      _plus<State, typename back<C>::type>,
      _minus<State, typename back<C>::type>
    >
  {};
};

struct build_mult
{
  template <class A, class B>
  class _mult
  {
  public:
    typedef _mult type;

    template <class T>
    T operator()(T t) const
    {
      return _left(t) * _right(t);
    }
  private:
    typename A::type _left;
    typename B::type _right;
  };

  template <class A, class B>
  class _div
  {
  public:
    typedef _div type;

    template <class T>
    T operator()(T t) const
    {
      return _left(t) / _right(t);
    }
  private:
    typename A::type _left;
    typename B::type _right;
  };

  template <class State, class C>
  struct apply :
    if_<
      typename is_c<front<C>, '*'>::type,
      _mult<State, typename back<C>::type>,
      _div<State, typename back<C>::type>
    >
  {};
};

struct build_value
{
  typedef build_value type;

  template <class V>
  struct apply
  {
    typedef apply type;

    template <class T>
    int operator()(T) const
    {
      return V::type::value;
    }
  };
};

struct arg
{
  typedef arg type;

  template <class T>
  T operator()(T t) const
  {
    return t;
  }
};

typedef
  one_of<transform<int_token, build_value>, always<arg_token, arg> >
  value_exp;

typedef
  foldl_reject_incomplete_start_with_parser<
    sequence<one_of<mult_token, div_token>, value_exp>,
    value_exp,
    build_mult
  >
  prod_exp;
  
typedef
  foldl_reject_incomplete_start_with_parser<
    sequence<one_of<plus_token, minus_token>, prod_exp>,
    prod_exp,
    build_plus
  >
  plus_exp;

typedef last_of<repeated<space>, plus_exp> expression;

typedef build_parser<entire_input<expression> > function_parser;

#ifdef BOOST_NO_CXX11_CONSTEXPR

template <class Exp>
struct lambda : apply_wrap1<function_parser, Exp> {};

using boost::metaparse::string;

lambda<string<'1','3'> >::type f1;
lambda<string<'2',' ','+',' ','3'> >::type f2;
lambda<string<'2',' ','*',' ','2'> >::type f3;
lambda<string<' ','1','+',' ','2','*','4','-','6','/','2'> >::type f4;
lambda<string<'2',' ','*',' ','_'> >::type f5;

#else

#ifdef LAMBDA
  #error LAMBDA already defined
#endif
#define LAMBDA(exp) apply_wrap1<function_parser, BOOST_METAPARSE_STRING(#exp)>::type

LAMBDA(13) f1;
LAMBDA(2 + 3) f2;
LAMBDA(2 * 2) f3;
LAMBDA( 1+ 2*4-6/2) f4;
LAMBDA(2 * _) f5;

#endif

int main()
{
  using std::cout;
  using std::endl;

  cout
    << f1(11) << endl
    << f2(11) << endl
    << f3(11) << endl
    << f4(11) << endl
    << f5(11) << endl
    << f5(1.1) << endl
    ;
}


