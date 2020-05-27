// Copyright Abel Sinkovics (abel@sinkovics.hu)  2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/foldl_reject_incomplete.hpp>
#include <boost/metaparse/foldl_reject_incomplete1.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/transform.hpp>
#include <boost/metaparse/one_char_except_c.hpp>
#include <boost/metaparse/one_of.hpp>
#include <boost/metaparse/always_c.hpp>
#include <boost/metaparse/build_parser.hpp>
#include <boost/metaparse/middle_of.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/detail/iterator.hpp>
#include <boost/xpressive/xpressive.hpp>

#include <boost/mpl/bool.hpp>
#include <boost/mpl/string.hpp>

using boost::metaparse::foldl_reject_incomplete;
using boost::metaparse::foldl_reject_incomplete1;
using boost::metaparse::lit_c;
using boost::metaparse::transform;
using boost::metaparse::build_parser;
using boost::metaparse::one_of;
using boost::metaparse::always_c;
using boost::metaparse::middle_of;
using boost::metaparse::one_char_except_c;
using boost::metaparse::entire_input;

using boost::mpl::c_str;
using boost::mpl::true_;
using boost::mpl::false_;

using boost::xpressive::sregex;
using boost::xpressive::as_xpr;

/*
 * Results of parsing
 */

template <class T>
struct has_value
{
  typedef T type;
  static const sregex value;
};

template <class T>
const sregex has_value<T>::value = T::run();

struct r_epsilon : has_value<r_epsilon>
{
  static sregex run()
  {
    return as_xpr("");
  }
};

struct r_any_char : has_value<r_any_char>
{
  static sregex run()
  {
    return boost::xpressive::_;
  }
};

struct r_char_lit
{
  template <class C>
  struct apply : has_value<apply<C> >
  {
    static sregex run()
    {
      return as_xpr(C::type::value);
    }
  };
};

struct r_append
{
  template <class A, class B>
  struct apply : has_value<apply<B, A> >
  {
    static sregex run()
    {
      return A::type::run() >> B::type::run();
    }
  };
};

/*
 * The grammar
 *
 * regexp ::= (bracket_expr | non_bracket_expr)*
 * non_bracket_expr ::= '.' | char_lit
 * bracket_expr ::= '(' regexp ')'
 * char_lit ::= any character except: . ( )
 */

typedef
  foldl_reject_incomplete1<
    one_of<
      always_c<'.', r_any_char>,
      transform<one_char_except_c<'.', '(', ')'>, r_char_lit>
    >,
    r_epsilon,
    r_append
  >
  non_bracket_expr;

typedef middle_of<lit_c<'('>, non_bracket_expr, lit_c<')'> > bracket_expr;

typedef
  foldl_reject_incomplete<
    one_of<bracket_expr, non_bracket_expr>,
    r_epsilon,
    r_append
  >
  regexp;

typedef build_parser<entire_input<regexp> > regexp_parser;

void test_string(const std::string& s)
{
  using boost::xpressive::regex_match;
  using boost::xpressive::smatch;
  using boost::mpl::apply_wrap1;

  using std::cout;
  using std::endl;

#if BOOST_METAPARSE_STD < 2011
  typedef boost::metaparse::string<'.','(','b','c',')'> regexp;
#else
  typedef BOOST_METAPARSE_STRING(".(bc)") regexp;
#endif

  const sregex re = apply_wrap1<regexp_parser, regexp>::type::value;
  smatch w;

  cout
    << s << (regex_match(s, w, re) ? " matches " : " doesn't match ")
    << c_str<regexp>::type::value
    << endl;
}

int main()
{
  test_string("abc");
  test_string("aba");
}


