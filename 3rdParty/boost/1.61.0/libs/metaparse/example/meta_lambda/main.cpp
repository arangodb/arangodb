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
#include <boost/mpl/plus.hpp>
#include <boost/mpl/minus.hpp>
#include <boost/mpl/times.hpp>
#include <boost/mpl/divides.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/equal_to.hpp>
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
using boost::mpl::plus;
using boost::mpl::minus;
using boost::mpl::times;
using boost::mpl::divides;
using boost::mpl::if_;
using boost::mpl::bool_;
using boost::mpl::equal_to;

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
  struct _plus
  {
    typedef _plus type;

    template <class T>
    struct apply :
      plus<typename apply_wrap1<A, T>::type, typename apply_wrap1<B, T>::type>
    {};
  };

  template <class A, class B>
  struct _minus
  {
    typedef _minus type;

    template <class T>
    struct apply :
      minus<typename apply_wrap1<A, T>::type, typename apply_wrap1<B, T>::type>
    {};
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
  struct _mult
  {
    typedef _mult type;

    template <class T>
    struct apply :
      times<typename apply_wrap1<A, T>::type, typename apply_wrap1<B, T>::type>
    {};
  };

  template <class A, class B>
  struct _div
  {
    typedef _div type;

    template <class T>
    struct apply :
      divides<
        typename apply_wrap1<A, T>::type,
        typename apply_wrap1<B, T>::type
      >
    {};
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

class build_value
{
private:
  template <class V>
  struct impl
  {
    typedef impl type;

    template <class T>
    struct apply : V {};
  };

public:
  typedef build_value type;

  template <class V>
  struct apply : impl<typename V::type> {};
};

struct arg
{
  typedef arg type;

  template <class T>
  struct apply
  {
    typedef T type;
  };
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

typedef build_parser<entire_input<expression> > metafunction_parser;

#ifdef BOOST_NO_CXX11_CONSTEXPR

template <class Exp>
struct meta_lambda : apply_wrap1<metafunction_parser, Exp> {};

int main()
{
  using std::cout;
  using std::endl;
  using boost::metaparse::string;

  typedef meta_lambda<string<'1','3'> >::type metafunction_class_1;
  typedef meta_lambda<string<'2',' ','+',' ','3'> >::type metafunction_class_2;
  typedef meta_lambda<string<'2',' ','*',' ','2'> >::type metafunction_class_3;
  typedef
    meta_lambda<string<' ','1','+',' ','2','*','4','-','6','/','2'> >::type
    metafunction_class_4;
  typedef meta_lambda<string<'2',' ','*',' ','_'> >::type metafunction_class_5;

  typedef boost::mpl::int_<11> int11;

  cout
    << apply_wrap1<metafunction_class_1, int11>::type::value << endl
    << apply_wrap1<metafunction_class_2, int11>::type::value << endl
    << apply_wrap1<metafunction_class_3, int11>::type::value << endl
    << apply_wrap1<metafunction_class_4, int11>::type::value << endl
    << apply_wrap1<metafunction_class_5, int11>::type::value << endl
    ;
}

#else

#ifdef META_LAMBDA
  #error META_LAMBDA already defined
#endif
#define META_LAMBDA(exp) \
  apply_wrap1<metafunction_parser, BOOST_METAPARSE_STRING(#exp)>::type

int main()
{
  using std::cout;
  using std::endl;

  typedef META_LAMBDA(13) metafunction_class_1;
  typedef META_LAMBDA(2 + 3) metafunction_class_2;
  typedef META_LAMBDA(2 * 2) metafunction_class_3;
  typedef META_LAMBDA( 1+ 2*4-6/2) metafunction_class_4;
  typedef META_LAMBDA(2 * _) metafunction_class_5;

  typedef boost::mpl::int_<11> int11;

  cout
    << apply_wrap1<metafunction_class_1, int11>::type::value << endl
    << apply_wrap1<metafunction_class_2, int11>::type::value << endl
    << apply_wrap1<metafunction_class_3, int11>::type::value << endl
    << apply_wrap1<metafunction_class_4, int11>::type::value << endl
    << apply_wrap1<metafunction_class_5, int11>::type::value << endl
    ;
}

#endif

