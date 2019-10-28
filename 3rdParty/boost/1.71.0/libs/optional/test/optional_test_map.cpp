// Copyright (C) 2018 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional/optional.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "boost/core/ignore_unused.hpp"
#include "boost/core/is_same.hpp"
#include "boost/core/lightweight_test.hpp"
#include "boost/core/lightweight_test_trait.hpp"


using boost::optional;
using boost::make_optional;
using boost::core::is_same;

template <typename Expected, typename Deduced>
void verify_type(Deduced)
{
  BOOST_TEST_TRAIT_TRUE(( is_same<Expected, Deduced> ));
}
  
#if (!defined BOOST_NO_CXX11_REF_QUALIFIERS) && (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES) 
struct MoveOnly
{
  int value;
  explicit MoveOnly(int i) : value(i) {}
  MoveOnly(MoveOnly && r) : value(r.value) { r.value = 0; }
  MoveOnly& operator=(MoveOnly && r) { value = r.value; r.value = 0; return *this; }
  
private:
  MoveOnly(MoveOnly const&);
  void operator=(MoveOnly const&);
};

MoveOnly makeMoveOnly(int i)
{
  return MoveOnly(i);
}

optional<MoveOnly> makeOptMoveOnly(int i)
{
  return optional<MoveOnly>(MoveOnly(i));
}

int get_val(MoveOnly m)
{
  return m.value;
}

void test_map_move_only()
{
  optional<MoveOnly> om (makeMoveOnly(7)), om2 (makeMoveOnly(8));
  verify_type<optional<int> >(boost::move(om).map(get_val));
  optional<int> oi = boost::move(om2).map(get_val);
  BOOST_TEST(bool(oi));
  BOOST_TEST_EQ(8, *oi);

  optional<int> oj = makeOptMoveOnly(4).map(get_val);
  BOOST_TEST(bool(oj));
  BOOST_TEST_EQ(4, *oj);
  
  optional<MoveOnly> o_;
  optional<int> oi_ = boost::move(o_).map(get_val);
  BOOST_TEST(!oi_);
}

#endif // no rvalue refs

struct Int
{
  int i;
  explicit Int(int i_) : i(i_) {}
};

struct convert_t
{
  typedef Int result_type;
  Int operator()(int i) { return Int(i); }
};

int& get_int_ref(Int& i)
{
  return i.i;
}

struct get_ref
{
  typedef int& result_type;
  int& operator()(int& i) { return i; }
};

void test_map()
{
  optional<int> oi (1);
  verify_type<optional<Int> >(oi.map(convert_t()));
  optional<Int> oI = oi.map(convert_t());
  BOOST_TEST(bool(oI));
  BOOST_TEST_EQ(1, oI->i);

  optional<Int> o_ = optional<int>().map(convert_t());
  BOOST_TEST(!o_);
}

optional<Int> make_opt_int(int i)
{
  if (i != 0)
    return Int(i);
  else
    return boost::none;
}

void test_map_optional()
{
  optional<int> o9 (9), o0 (0), o_;
  verify_type<optional<optional<Int> > >(o9.map(make_opt_int));
  optional<optional<Int> > oo9 = o9.map(make_opt_int);
  BOOST_TEST(bool(oo9));  
  BOOST_TEST(bool(*oo9));  
  BOOST_TEST_EQ(9, (**oo9).i);

  optional<optional<Int> > oo0 = o0.map(make_opt_int);
  BOOST_TEST(bool(oo0));  
  BOOST_TEST(!*oo0);  
  
  optional<optional<Int> > oo_ = o_.map(make_opt_int);
  BOOST_TEST(!oo_);  
}

void test_map_with_lambda()
{
#if !defined BOOST_NO_CXX11_LAMBDAS && !defined BOOST_NO_CXX11_DECLTYPE_N3276
  optional<int> oi (1), oj(2);
  verify_type<optional<bool> >(oi.map([](int i){ return i == 1; }));
  optional<bool> ob = oi.map([](int i){ return i == 1; });
  optional<bool> oc = oj.map([](int i){ return i == 1; });
  BOOST_TEST(bool(ob));
  BOOST_TEST_EQ(true, *ob);
  BOOST_TEST(bool(oc));
  BOOST_TEST_EQ(false, *oc);
#endif // lambdas
}

void test_map_to_ref()
{
  optional<int> oi (2);
  verify_type<optional<int&> >(oi.map(get_ref()));
  optional<int&> ori = oi.map(get_ref());
  BOOST_TEST(bool(ori));
  *ori = 3;
  BOOST_TEST(bool(oi));
  BOOST_TEST_EQ(3, *oi);
  BOOST_TEST_EQ(3, *ori);
}

void test_map_optional_ref()
{
  Int I (5);
  optional<Int&> ori (I);
  verify_type<optional<int&> >(ori.map(get_int_ref));
  optional<int&> orii = ori.map(get_int_ref);
  BOOST_TEST(bool(orii));
  BOOST_TEST_EQ(5, *orii);
  *orii = 6;
  BOOST_TEST_EQ(6, I.i);
}

int main()
{
#if (!defined BOOST_NO_CXX11_REF_QUALIFIERS) && (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES) 
  test_map_move_only();
#endif
  test_map_with_lambda();
	test_map();
  test_map_optional();
  test_map_to_ref();
  test_map_optional();
  test_map_optional_ref();

  return boost::report_errors();
}
