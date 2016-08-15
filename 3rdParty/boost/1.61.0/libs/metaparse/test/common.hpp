#ifndef BOOST_PARSER_TEST_COMMON_H
#define BOOST_PARSER_TEST_COMMON_H

// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/lit.hpp>
#include <boost/metaparse/lit_c.hpp>

#include <boost/mpl/list.hpp>
#include <boost/mpl/list_c.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/equal.hpp>

#include <string>

typedef boost::mpl::list_c<char> str_;
typedef boost::mpl::list_c<char, '0'> str_0;
typedef boost::mpl::list_c<char, '1'> str_1;
typedef boost::mpl::list_c<char, '1', '9', '8', '3'> str_1983;
typedef boost::mpl::list_c<char, 'a'> str_a;
typedef boost::mpl::list_c<char, 'a', 'b'> str_ab;
typedef boost::mpl::list_c<char, 'a', 'a', 'a', 'a', 'b'> str_aaaab;
typedef boost::mpl::list_c<char, 'a', 'c'> str_ac;
typedef boost::mpl::list_c<char, 'b'> str_b;
typedef boost::mpl::list_c<char, 'b', 'a'> str_ba;
typedef boost::mpl::list_c<char, 'b', 'a', 'a', 'a', 'a'> str_baaaa;
typedef boost::mpl::list_c<char, 'c'> str_c;
typedef boost::mpl::list_c<char, 'c', 'a'> str_ca;
typedef boost::mpl::list_c<char, 'h'> str_h;
typedef boost::mpl::list_c<char, 'e'> str_e;
typedef boost::mpl::list_c<char, 'l'> str_l;
typedef boost::mpl::list_c<char, 'b', 'e', 'l', 'l', 'o'> str_bello;
typedef boost::mpl::list_c<char, 'h', 'e', 'l', 'l', 'o'> str_hello;
typedef boost::mpl::list_c<char, ' ', 'e', 'l', 'l', 'o'> str__ello;

typedef boost::mpl::list_c<char, '0', 'e', 'l', 'l', 'o'> chars0;
typedef boost::mpl::list_c<char, 'h', '0', 'l', 'l', 'o'> chars1;
typedef boost::mpl::list_c<char, 'h', 'e', '0', 'l', 'o'> chars2;
typedef boost::mpl::list_c<char, 'h', 'e', 'l', '0', 'o'> chars3;
typedef boost::mpl::list_c<char, 'h', 'e', 'l', 'l', '0'> chars4;
typedef boost::mpl::list_c<char, 'h', 'e', 'l', 'l', 'o'> chars5;

typedef boost::mpl::char_<'0'> char_0;
typedef boost::mpl::char_<'1'> char_1;
typedef boost::mpl::char_<'7'> char_7;
typedef boost::mpl::char_<'9'> char_9;
typedef boost::mpl::char_<'a'> char_a;
typedef boost::mpl::char_<'b'> char_b;
typedef boost::mpl::char_<'e'> char_e;
typedef boost::mpl::char_<'h'> char_h;
typedef boost::mpl::char_<'k'> char_k;
typedef boost::mpl::char_<'K'> char_K;
typedef boost::mpl::char_<'l'> char_l;
typedef boost::mpl::char_<'o'> char_o;
typedef boost::mpl::char_<'x'> char_x;

typedef boost::mpl::char_<' '> char_space;
typedef boost::mpl::char_<'\t'> char_tab;
typedef boost::mpl::char_<'\n'> char_new_line;
typedef boost::mpl::char_<'\r'> char_cret;

typedef boost::mpl::int_<0> int0;
typedef boost::mpl::int_<1> int1;
typedef boost::mpl::int_<2> int2;
typedef boost::mpl::int_<3> int3;
typedef boost::mpl::int_<9> int9;
typedef boost::mpl::int_<10> int10;
typedef boost::mpl::int_<11> int11;
typedef boost::mpl::int_<12> int12;
typedef boost::mpl::int_<13> int13;
typedef boost::mpl::int_<14> int14;
typedef boost::mpl::int_<28> int28;

typedef boost::metaparse::lit<char_e> lit_e;
typedef boost::metaparse::lit<char_h> lit_h;
typedef boost::metaparse::lit<char_l> lit_l;
typedef boost::metaparse::lit<char_x> lit_x;

typedef boost::metaparse::lit_c<'e'> lit_c_e;
typedef boost::metaparse::lit_c<'h'> lit_c_h;

typedef boost::mpl::list< > empty_list;

typedef
  boost::mpl::at<boost::mpl::vector<int, double>, int11>
  can_not_be_instantiated;

struct test_failure
{
  typedef test_failure type;

  static std::string get_value() { return "fail"; }
};

struct equal_sequences
{
  typedef equal_sequences type;

  template <class A, class B>
  struct apply : boost::mpl::equal<typename A::type, typename B::type> {};
};

#endif

