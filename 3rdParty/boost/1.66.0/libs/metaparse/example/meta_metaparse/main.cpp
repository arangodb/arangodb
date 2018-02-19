// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_MPL_LIMIT_STRING_SIZE 64
#define BOOST_METAPARSE_LIMIT_STRING_SIZE BOOST_MPL_LIMIT_STRING_SIZE

#include <boost/metaparse/grammar.hpp>

#include <boost/metaparse/string.hpp>
#include <boost/metaparse/build_parser.hpp>
#include <boost/metaparse/token.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/int_.hpp>
#include <boost/metaparse/transform.hpp>

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/fold.hpp>

using boost::mpl::apply_wrap1;
using boost::mpl::front;
using boost::mpl::back;
using boost::mpl::if_;
using boost::mpl::bool_;

using boost::metaparse::build_parser;
using boost::metaparse::token;
using boost::metaparse::entire_input;
using boost::metaparse::int_;
using boost::metaparse::grammar;
using boost::metaparse::transform;

#if BOOST_METAPARSE_STD < 2011
int main()
{
  std::cout << "Please use a compiler that supports constexpr" << std::endl;
}
#else

#ifdef _STR
#  error _STR already defined
#endif
#define _STR BOOST_METAPARSE_STRING

template <class T, char C>
struct is_c : bool_<T::type::value == C> {};

struct build_plus_impl
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

struct build_plus
{
  typedef build_plus type;

  template <class Seq>
  struct apply :
    boost::mpl::fold<
      typename back<Seq>::type,
      typename front<Seq>::type,
      build_plus_impl
    >
  {};
};

struct build_mult_impl
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

struct build_mult
{
  typedef build_mult type;

  template <class Seq>
  struct apply :
    boost::mpl::fold<
      typename back<Seq>::type,
      typename front<Seq>::type,
      build_mult_impl
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

struct build_arg
{
  typedef build_arg type;

  template <class>
  struct apply
  {
    typedef apply type;
  
    template <class T>
    T operator()(T t) const
    {
      return t;
    }
  };
};

struct keep_front
{
  typedef keep_front type;

  template <class Seq>
  struct apply : front<Seq> {};
};

typedef
  grammar<_STR("plus_exp")>
    ::import<_STR("int_token"), token<transform<int_, build_value>>>::type

    ::rule<_STR("ws ::= (' ' | '\n' | '\r' | '\t')*")>::type
    ::rule<_STR("plus_token ::= '+' ws"), keep_front>::type
    ::rule<_STR("minus_token ::= '-' ws"), keep_front>::type
    ::rule<_STR("mult_token ::= '*' ws"), keep_front>::type
    ::rule<_STR("div_token ::= '/' ws"), keep_front>::type
    ::rule<_STR("arg_token ::= '_' ws"), keep_front>::type

    ::rule<_STR("plus_exp ::= prod_exp ((plus_token | minus_token) prod_exp)*"), build_plus>::type
    ::rule<_STR("prod_exp ::= value_exp ((mult_token | div_token) value_exp)*"), build_mult>::type
    ::rule<_STR("value_exp ::= int_token | arg_exp")>::type
    ::rule<_STR("arg_exp ::= arg_token"), build_arg>::type
  g;

typedef build_parser<entire_input<g>> function_parser;

#ifdef LAMBDA
  #error LAMBDA already defined
#endif
#define LAMBDA(exp) apply_wrap1<function_parser, _STR(#exp)>::type

LAMBDA(13) f1;
LAMBDA(2 + 3) f2;
LAMBDA(2 * 3) f3;
LAMBDA(1+ 2*4-6/2) f4;
LAMBDA(2 * _) f5;

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

#endif


