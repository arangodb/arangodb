// Copyright Abel Sinkovics (abel@sinkovics.hu)  2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/sequence.hpp>
#include <boost/metaparse/lit_c.hpp>
#include <boost/metaparse/foldl.hpp>
#include <boost/metaparse/entire_input.hpp>
#include <boost/metaparse/string.hpp>
#include <boost/metaparse/build_parser.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/last_of.hpp>
#include <boost/metaparse/iterate_c.hpp>
#include <boost/metaparse/one_char.hpp>
#include <boost/metaparse/return_.hpp>

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/plus.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/size.hpp>

#include <iostream>
#include <string>

#if BOOST_METAPARSE_STD < 2011

int main()
{
  std::cout
    << "This example focuses on constexpr, which is not supported"
    << std::endl;
}

#else

using boost::metaparse::sequence;
using boost::metaparse::lit_c;
using boost::metaparse::build_parser;
using boost::metaparse::foldl;
using boost::metaparse::entire_input;
using boost::metaparse::get_result;
using boost::metaparse::start;
using boost::metaparse::last_of;
using boost::metaparse::iterate_c;
using boost::metaparse::one_char;
using boost::metaparse::return_;

using boost::mpl::apply_wrap1;
using boost::mpl::apply_wrap2;
using boost::mpl::plus;
using boost::mpl::int_;
using boost::mpl::at_c;
using boost::mpl::front;
using boost::mpl::pop_front;
using boost::mpl::size;

using std::ostream;

/*
 * The grammar
 *
 * S ::= a* b* a*
 */

struct parsed
{
  template <class T>
  constexpr static parsed build()
  {
    return
      parsed(
        at_c<typename T::type, 0>::type::value,
        at_c<typename T::type, 1>::type::value,
        at_c<typename T::type, 2>::type::value
      );
  }

  constexpr parsed(int a1, int b, int a2) :
    a_count1(a1),
    b_count(b),
    a_count2(a2)
  {};

  int a_count1;
  int b_count;
  int a_count2;
};

ostream& operator<<(ostream& o, const parsed& p)
{
  return
    o << "(" << p.a_count1 << ", " << p.b_count << ", " << p.a_count2 << ")";
}

// constexpr parser

template <class T, int Len>
struct const_list
{
  T head;
  const_list<T, Len - 1> tail;

  constexpr const_list(const T& h, const const_list<T, Len - 1>& t) :
    head(h),
    tail(t)
  {}

  constexpr const_list<T, Len + 1> push_front(const T& t) const
  {
    return const_list<T, Len + 1>(t, *this);
  }

  constexpr T at(int n) const
  {
    return n == 0 ? head : tail.at(n - 1);
  }
};

template <class T>
struct const_list<T, 0>
{
  constexpr const_list<T, 1> push_front(const T& t) const
  {
    return const_list<T, 1>(t, *this);
  }

  constexpr T at(int) const
  {
    return T();
  }
};

template <class T, int N, int from = 0>
struct to_list
{
  static constexpr const_list<T, N - from> run(const T (&s)[N])
  {
    return const_list<T, N - from>(s[from], to_list<T, N, from + 1>::run(s));
  }
};

template <class T, int N>
struct to_list<T, N, N>
{
  static constexpr const_list<T, 0> run(const T (&s)[N])
  {
    return const_list<T, 0>();
  }
};

struct presult
{
  int value;
  int remaining_from;

  constexpr presult(int v, int r) : value(v), remaining_from(r) {}

  constexpr presult incr() const { return presult(value + 1, remaining_from); }
};

template <char C, int N>
constexpr presult parse_cs(const const_list<char, N>& s, int from)
{
  return
    from < N ?
    (s.at(from) == C ? parse_cs<C, N>(s, from + 1).incr() : presult(0, from)) :
    presult(0, from);
}

template <int N>
constexpr parsed parse_impl(const const_list<char, N>& s)
{
  return
    parsed(
      parse_cs<'a', N>(s, 0).value,
      parse_cs<'b', N>(s, parse_cs<'a', N>(s, 0).remaining_from).value,
      parse_cs<'a', N>(
        s,
        parse_cs<'b', N>(
          s,
          parse_cs<'a', N>(s, 0).remaining_from
        ).remaining_from
      ).value
    );
}

template <int N>
constexpr parsed parse(const char (&s)[N])
{
  return parse_impl(to_list<char, N>::run(s));
}

// TMP parser

struct count
{
  typedef count type;

  template <class State, class C>
  struct apply : plus<int_<1>, State> {};
};

typedef foldl<lit_c<'a'>, int_<0>, count> as;
typedef foldl<lit_c<'b'>, int_<0>, count> bs;

typedef sequence<as, bs, as> s;

typedef build_parser<entire_input<s> > parser;

#ifdef P
  #error P already defined
#endif
#define P(x) \
  parsed::build<boost::mpl::apply_wrap1<parser, BOOST_METAPARSE_STRING(#x)> >()

// Mixed parser

template <class ValueType, class L, int Len>
struct tmp_to_const_list_impl
{
  constexpr static const_list<ValueType, Len> run()
  {
    return
      const_list<ValueType, Len>(
        front<L>::type::value,
        tmp_to_const_list_impl<
          ValueType,
          typename pop_front<L>::type,
          Len - 1
        >::run()
      );
  }
};

template <class ValueType, class L>
struct tmp_to_const_list_impl<ValueType, L, 0>
{
  constexpr static const_list<ValueType, 0> run()
  {
    return const_list<ValueType, 0>();
  }
};

template <class ValueType, class L>
struct tmp_to_const_list :
  tmp_to_const_list_impl<ValueType, L, size<L>::type::value>
{};

template <char C>
struct parse_with_constexpr
{
  typedef parse_with_constexpr type;

  template <class S>
  static constexpr presult impl()
  {
    return
      parse_cs<C, size<S>::type::value>(tmp_to_const_list<char, S>::run(), 0);
  }

  template <class S, class Pos>
  struct apply :
    apply_wrap2<
      last_of<
        iterate_c<one_char, impl<S>().remaining_from>,
        return_<int_<impl<S>().value> >
      >,
      S,
      Pos
    >
  {};
};

typedef parse_with_constexpr<'b'> bs_mixed;

typedef sequence<as, bs_mixed, as> s_mixed;

typedef build_parser<entire_input<s_mixed> > parser_mixed;

#ifdef P_MIXED
  #error P_MIXED already defined
#endif
#define P_MIXED(x) \
  parsed::build< \
    boost::mpl::apply_wrap1<parser_mixed, BOOST_METAPARSE_STRING(#x)> \
  >()

int main()
{
  using std::cout;
  using std::endl;
  
  cout
    << "TMP only parsers:" << endl
    << P(aba) << endl
    << P(aaaaaaabbbbaaaa) << endl
    << endl

    << "constexpr only parsers:" << endl
    << parse("") << endl
    << parse("aba") << endl
    << parse("aaaaaaabbbbaaaa") << endl
    << endl

    << "mixed parsers:" << endl
    << P_MIXED(aba) << endl
    << P_MIXED(aaaaaaabbbbaaaa) << endl
    << endl
    ;
}

#endif

