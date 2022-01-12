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

#ifdef BOOST_BORLANDC
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

struct Int
{
  int i;
  explicit Int(int i_) : i(i_) {}
};

struct convert_t
{
  typedef optional<Int> result_type;
  optional<Int> operator()(int i) { if (i != 0) return Int(i); else return boost::none; }
};

void test_flat_map_on_mutable_optional_with_function_object()
{
  {
    optional<int> oi (1);
    verify_type< optional<Int> >(oi.flat_map(convert_t()));
    optional<Int> oI = oi.flat_map(convert_t());
    BOOST_TEST(bool(oI));
    BOOST_TEST_EQ(1, oI->i);
  }
  {
    optional<int> oi (0);
    optional<Int> oI = oi.flat_map(convert_t());
    BOOST_TEST(!oI);
  }
  {
    optional<int> oi;
    optional<Int> oI = oi.flat_map(convert_t());
    BOOST_TEST(!oI);
  }
}

void test_flat_map_on_const_optional_with_function_object()
{
  {
    const optional<int> oi (1);
    verify_type< optional<Int> >(oi.flat_map(convert_t()));
    optional<Int> oI = oi.flat_map(convert_t());
    BOOST_TEST(bool(oI));
    BOOST_TEST_EQ(1, oI->i);
  }
  {
    const optional<int> oi (0);
    optional<Int> oI = oi.flat_map(convert_t());
    BOOST_TEST(!oI);
  }
  {
    const optional<int> oi;
    optional<Int> oI = oi.flat_map(convert_t());
    BOOST_TEST(!oI);
  }
}

void test_flat_map_with_lambda()
{
#if !defined BOOST_NO_CXX11_LAMBDAS && !defined BOOST_NO_CXX11_DECLTYPE_N3276
  {
    optional<int> oi (1);
    verify_type< optional<Int> >(oi.flat_map([](int i){ return optional<Int>(i == 0, Int(i)); }));
    optional<Int> oI = oi.flat_map([](int i){ return optional<Int>(i != 0, Int(i)); });
    BOOST_TEST(bool(oI));
    BOOST_TEST_EQ(1, oI->i);
  }
  {
    optional<int> oi (0);
    optional<Int> oI = oi.flat_map([](int i){ return optional<Int>(i != 0, Int(i)); });
    BOOST_TEST(!oI);
  }
  {
    optional<int> oi;
    optional<Int> oI = oi.flat_map([](int i){ return optional<Int>(i != 0, Int(i)); });
    BOOST_TEST(!oI);
  }
#endif // lambdas
}

struct get_opt_ref
{
  typedef optional<int&> result_type;
  optional<int&> operator()(int& i) { return i != 0 ? optional<int&>(i) : optional<int&>(); }
};

void test_flat_map_obj_to_ref()
{
  {
    optional<int> oi (2);
    verify_type< optional<int&> >(oi.flat_map(get_opt_ref()));
    optional<int&> ori = oi.flat_map(get_opt_ref());
    BOOST_TEST(bool(ori));
    BOOST_TEST_EQ(2, *ori);
    *ori = 3;
    BOOST_TEST(bool(oi));
    BOOST_TEST_EQ(3, *oi);
    BOOST_TEST_EQ(3, *ori);
  }
  {
    optional<int> oi (0);
    optional<int&> ori = oi.flat_map(get_opt_ref());
    BOOST_TEST(!ori);
  }
  {
    optional<int> oi;
    optional<int&> ori = oi.flat_map(get_opt_ref());
    BOOST_TEST(!ori);
  }
}

optional<int&> get_opt_int_ref(Int& i)
{
  return i.i ? optional<int&>(i.i) : optional<int&>();
}

void test_flat_map_ref_to_ref()
{
  {
    Int I (5);
    optional<Int&> orI (I);
    verify_type< optional<int&> >(orI.flat_map(get_opt_int_ref));
    optional<int&> ori = orI.flat_map(get_opt_int_ref);
    BOOST_TEST(bool(ori));
    BOOST_TEST_EQ(5, *ori);
    *ori = 6;
    BOOST_TEST_EQ(6, *ori);
    BOOST_TEST_EQ(6, I.i);
  }
  {
    Int I (0);
    optional<Int&> orI (I);
    optional<int&> ori = orI.flat_map(get_opt_int_ref);
    BOOST_TEST(!ori);
  }
  {
    optional<Int&> orI;
    optional<int&> ori = orI.flat_map(get_opt_int_ref);
    BOOST_TEST(!ori);
  }
}

optional< optional<Int> > make_opt_int(int i)
{
  if (i == 0)
    return boost::none;
  else if (i == 1)
    return boost::make_optional(optional<Int>());
  else
    return boost::make_optional(boost::make_optional(Int(i)));
}

void test_flat_map_opt_opt()
{
  {
    optional<int> oi (9);
    verify_type<optional<optional<Int> > >(oi.flat_map(make_opt_int));
    optional<optional<Int> > ooI = oi.flat_map(make_opt_int);
    BOOST_TEST(bool(ooI));
    BOOST_TEST(bool(*ooI));
    BOOST_TEST_EQ(9, (**ooI).i);
  }
  {
    optional<int> oi (1);
    optional<optional<Int> > ooI = oi.flat_map(make_opt_int);
    BOOST_TEST(bool(ooI));
    BOOST_TEST(!*ooI);
  }
  {
    optional<int> oi (0);
    optional<optional<Int> > ooI = oi.flat_map(make_opt_int);
    BOOST_TEST(!ooI);
  }
  {
    optional<int> oi;
    optional<optional<Int> > ooI = oi.flat_map(make_opt_int);
    BOOST_TEST(!ooI);
  }
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

optional<int> get_val(MoveOnly m)
{
  return optional<int>(m.value != 0, m.value);
}

void test_flat_map_move_only()
{
  {
    optional<MoveOnly> om (makeMoveOnly(1)), om2 (makeMoveOnly(2));
    verify_type<optional<int> >(boost::move(om).flat_map(get_val));
    optional<int> oi = boost::move(om2).flat_map(get_val);
    BOOST_TEST(bool(oi));
    BOOST_TEST_EQ(2, *oi);
  }
  {
    optional<int> oj = makeOptMoveOnly(4).flat_map(get_val);
    BOOST_TEST(bool(oj));
    BOOST_TEST_EQ(4, *oj);
  }
  {
    optional<int> oj = optional<MoveOnly>().flat_map(get_val);
    BOOST_TEST(!oj);
  }
}

#endif // no rvalue refs

int main()
{
  test_flat_map_on_mutable_optional_with_function_object();
  test_flat_map_on_const_optional_with_function_object();
  test_flat_map_with_lambda();
  test_flat_map_obj_to_ref();
  test_flat_map_ref_to_ref();
  test_flat_map_opt_opt();

#if (!defined BOOST_NO_CXX11_REF_QUALIFIERS) && (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES)
  test_flat_map_move_only();
#endif

  return boost::report_errors();
}
