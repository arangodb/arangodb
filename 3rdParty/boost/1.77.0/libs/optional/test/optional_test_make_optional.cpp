// Copyright (C) 2017 Andrzej Krzemienski.
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
  
#if (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES)
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

void test_make_optional_for_move_only_type()
{
  verify_type< optional<MoveOnly> >(make_optional(makeMoveOnly(2)));
  verify_type< optional<MoveOnly> >(make_optional(true, makeMoveOnly(2)));
  
  optional<MoveOnly> o1 = make_optional(makeMoveOnly(1));
  BOOST_TEST (o1);
  BOOST_TEST_EQ (1, o1->value);
  
  optional<MoveOnly> o2 = make_optional(true, makeMoveOnly(2));
  BOOST_TEST (o2);
  BOOST_TEST_EQ (2, o2->value);
  
  optional<MoveOnly> oN = make_optional(false, makeMoveOnly(2));
  BOOST_TEST (!oN);
}

#endif // !defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES

void test_make_optional_for_optional()
{
  optional<int> oi;
  verify_type< optional< optional<int> > >(make_optional(oi));
  verify_type< optional< optional<int> > >(make_optional(true, oi));
  
  optional< optional<int> > ooi = make_optional(oi);
  BOOST_TEST (ooi);
  BOOST_TEST (!*ooi);
  
  optional< optional<int> > ooT = make_optional(true, oi);
  BOOST_TEST (ooT);
  BOOST_TEST (!*ooT);
  
  optional< optional<int> > ooF = make_optional(false, oi);
  BOOST_TEST (!ooF);
}

void test_nested_make_optional()
{
  verify_type< optional< optional<int> > >(make_optional(make_optional(1)));
  verify_type< optional< optional<int> > >(make_optional(true, make_optional(true, 2)));
  
  optional< optional<int> > oo1 = make_optional(make_optional(1));
  BOOST_TEST (oo1);
  BOOST_TEST (*oo1);
  BOOST_TEST_EQ (1, **oo1);
  
  optional< optional<int> > oo2 = make_optional(true, make_optional(true, 2));
  BOOST_TEST (oo2);
  BOOST_TEST (*oo2);
  BOOST_TEST_EQ (2, **oo2);
  
  optional< optional<int> > oo3 = make_optional(true, make_optional(false, 3));
  BOOST_TEST (oo3);
  BOOST_TEST (!*oo3);
  
  optional< optional<int> > oo4 = make_optional(false, make_optional(true, 4));
  BOOST_TEST (!oo4);
}

int main()
{
#if (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES)
  test_make_optional_for_move_only_type();
#endif
  test_make_optional_for_optional();
  test_nested_make_optional();

  return boost::report_errors();
}
