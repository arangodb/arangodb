// Boost.Polygon library voronoi_ctypes_test.cpp file

//          Copyright Andrii Sydorchuk 2010-2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#include <boost/core/lightweight_test.hpp>
#include <boost/polygon/detail/voronoi_ctypes.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <vector>
#include <ctime>

using namespace boost::polygon::detail;

type_converter_fpt to_fpt;

void ulp_comparison_test1()
{
  ulp_comparison<double> ulp_cmp;
  uint64 a = 22;
  uint64 b = 27;
  fpt64 da, db;
  std::memcpy(&da, &a, sizeof(uint64));
  std::memcpy(&db, &b, sizeof(uint64));
  BOOST_TEST_EQ(ulp_cmp(da, db, 1), ulp_cmp.LESS);
  BOOST_TEST_EQ(ulp_cmp(db, da, 1), ulp_cmp.MORE);
  BOOST_TEST_EQ(ulp_cmp(da, db, 4), ulp_cmp.LESS);
  BOOST_TEST_EQ(ulp_cmp(da, db, 5), ulp_cmp.EQUAL);
  BOOST_TEST_EQ(ulp_cmp(da, db, 6), ulp_cmp.EQUAL);
}

void ulp_comparison_test2()
{
  ulp_comparison<fpt64> ulp_cmp;
  uint64 a = 0ULL;
  uint64 b = 0x8000000000000002ULL;
  fpt64 da, db;
  std::memcpy(&da, &a, sizeof(uint64));
  std::memcpy(&db, &b, sizeof(uint64));
  BOOST_TEST_EQ(ulp_cmp(da, db, 1), ulp_cmp.MORE);
  BOOST_TEST_EQ(ulp_cmp(db, da, 1), ulp_cmp.LESS);
  BOOST_TEST_EQ(ulp_cmp(da, db, 2), ulp_cmp.EQUAL);
  BOOST_TEST_EQ(ulp_cmp(da, db, 3), ulp_cmp.EQUAL);
}

void extended_exponent_fpt_test1()
{
  boost::mt19937_64 gen(static_cast<uint32>(time(NULL)));
  fpt64 b = 0.0;
  efpt64 eeb(b);
  for (int i = 0; i < 1000; ++i) {
    fpt64 a = to_fpt(static_cast<int64>(gen()));
    efpt64 eea(a);
    efpt64 neg = -eea;
    efpt64 sum = eea + eeb;
    efpt64 dif = eea - eeb;
    efpt64 mul = eea * eeb;
    BOOST_TEST_EQ(to_fpt(neg), -a);
    BOOST_TEST_EQ(to_fpt(sum), a + b);
    BOOST_TEST_EQ(to_fpt(dif), a - b);
    BOOST_TEST_EQ(to_fpt(mul), a * b);
  }
}

void extended_exponent_fpt_test2()
{
  boost::mt19937_64 gen(static_cast<uint32>(time(NULL)));
  fpt64 a = 0.0;
  efpt64 eea(a);
  for (int i = 0; i < 1000; ++i) {
    fpt64 b = to_fpt(static_cast<int64>(gen()));
    if (b == 0.0)
      continue;
    efpt64 eeb(b);
    efpt64 neg = -eea;
    efpt64 sum = eea + eeb;
    efpt64 dif = eea - eeb;
    efpt64 mul = eea * eeb;
    efpt64 div = eea / eeb;
    BOOST_TEST_EQ(to_fpt(neg), -a);
    BOOST_TEST_EQ(to_fpt(sum), a + b);
    BOOST_TEST_EQ(to_fpt(dif), a - b);
    BOOST_TEST_EQ(to_fpt(mul), a * b);
    BOOST_TEST_EQ(to_fpt(div), a / b);
  }
}

void extended_exponent_fpt_test3()
{
  boost::mt19937_64 gen(static_cast<uint32>(time(NULL)));
  for (int i = 0; i < 1000; ++i) {
    fpt64 a = to_fpt(static_cast<int64>(gen()));
    fpt64 b = to_fpt(static_cast<int64>(gen()));
    if (b == 0.0)
      continue;
    efpt64 eea(a);
    efpt64 eeb(b);
    efpt64 neg = -eea;
    efpt64 sum = eea + eeb;
    efpt64 dif = eea - eeb;
    efpt64 mul = eea * eeb;
    efpt64 div = eea / eeb;
    BOOST_TEST_EQ(to_fpt(neg), -a);
    BOOST_TEST_EQ(to_fpt(sum), a + b);
    BOOST_TEST_EQ(to_fpt(dif), a - b);
    BOOST_TEST_EQ(to_fpt(mul), a * b);
    BOOST_TEST_EQ(to_fpt(div), a / b);
  }
}

void extended_exponent_fpt_test4()
{
  for (int exp = 0; exp < 64; ++exp)
  for (int i = 1; i < 100; ++i) {
    fpt64 a = i;
    fpt64 b = to_fpt(1LL << exp);
    efpt64 eea(a);
    efpt64 eeb(b);
    efpt64 neg = -eea;
    efpt64 sum = eea + eeb;
    efpt64 dif = eea - eeb;
    efpt64 mul = eea * eeb;
    efpt64 div = eea / eeb;
    BOOST_TEST_EQ(to_fpt(neg), -a);
    BOOST_TEST_EQ(to_fpt(sum), a + b);
    BOOST_TEST_EQ(to_fpt(dif), a - b);
    BOOST_TEST_EQ(to_fpt(mul), a * b);
    BOOST_TEST_EQ(to_fpt(div), a / b);
  }
}

void extended_exponent_fpt_test5()
{
  for (int i = 0; i < 100; ++i) {
    efpt64 a(to_fpt(i * i));
    efpt64 b = a.sqrt();
    BOOST_TEST_EQ(to_fpt(b), to_fpt(i));
  }
}

void extended_exponent_fpt_test6()
{
  for (int i = -10; i <= 10; ++i) {
    efpt64 a(to_fpt(i));
    BOOST_TEST_EQ(is_pos(a), i > 0);
    BOOST_TEST_EQ(is_neg(a), i < 0);
    BOOST_TEST_EQ(is_zero(a), !i);
  }
}

void extended_int_test1()
{
  typedef extended_int<1> eint32;
  eint32 e1(0), e2(32), e3(-32);
  BOOST_TEST_EQ(e1.count(), 0);
  BOOST_TEST_EQ(e1.size(), 0U);
  BOOST_TEST_EQ(e2.count(), 1);
  BOOST_TEST_EQ(e2.chunks()[0], 32U);
  BOOST_TEST_EQ(e2.size(), 1U);
  BOOST_TEST_EQ(e3.count(), -1);
  BOOST_TEST_EQ(e3.chunks()[0], 32U);
  BOOST_TEST_EQ(e3.size(), 1U);
}

void extended_int_test2()
{
  typedef extended_int<2> eint64;
  int64 val64 = 0x7fffffffffffffffLL;
  eint64 e1(0), e2(32), e3(-32), e4(val64), e5(-val64);
  BOOST_TEST_EQ(e1.count(), 0);
  BOOST_TEST_EQ(e2.count(), 1);
  BOOST_TEST_EQ(e2.chunks()[0], 32U);
  BOOST_TEST_EQ(e3.count(), -1);
  BOOST_TEST_EQ(e3.chunks()[0], 32U);
  BOOST_TEST_EQ(e4.count(), 2);
  BOOST_TEST_EQ(e4.chunks()[0], 0xffffffff);
  BOOST_TEST_EQ(e4.chunks()[1], val64 >> 32);
  BOOST_TEST_EQ(e5.count(), -2);
  BOOST_TEST_EQ(e5.chunks()[0], 0xffffffff);
  BOOST_TEST_EQ(e5.chunks()[1], val64 >> 32);
}

void extended_int_test3()
{
  typedef extended_int<2> eint64;
  std::vector<uint32> chunks;
  chunks.push_back(1);
  chunks.push_back(2);
  eint64 e1(chunks, true), e2(chunks, false);
  BOOST_TEST_EQ(e1.count(), 2);
  BOOST_TEST_EQ(e1.chunks()[0], 2U);
  BOOST_TEST_EQ(e1.chunks()[1], 1U);
  BOOST_TEST_EQ(e2.count(), -2);
  BOOST_TEST_EQ(e2.chunks()[0], 2U);
  BOOST_TEST_EQ(e2.chunks()[1], 1U);
}

void extended_int_test4()
{
  typedef extended_int<2> eint64;
  std::vector<uint32> chunks;
  chunks.push_back(1);
  chunks.push_back(2);
  eint64 e1(chunks, true), e2(chunks, false);
  BOOST_TEST_EQ(e1 == e2, false);
  BOOST_TEST_EQ(e1 == -e2, true);
  BOOST_TEST_EQ(e1 != e2, true);
  BOOST_TEST_EQ(e1 != -e2, false);
  BOOST_TEST_EQ(e1 < e2, false);
  BOOST_TEST_EQ(e1 < -e2, false);
  BOOST_TEST_EQ(e1 <= e2, false);
  BOOST_TEST_EQ(e1 <= -e2, true);
  BOOST_TEST_EQ(e1 > e2, true);
  BOOST_TEST_EQ(e1 > -e2, false);
  BOOST_TEST_EQ(e1 >= e2, true);
  BOOST_TEST_EQ(e1 >= -e2, true);
}

void extended_int_test5()
{
  typedef extended_int<2> eint64;
  boost::mt19937_64 gen(static_cast<uint32>(time(NULL)));
  for (int i = 0; i < 1000; ++i) {
    int64 i1 = static_cast<int64>(gen());
    int64 i2 = static_cast<int64>(gen());
    eint64 e1(i1), e2(i2);
    BOOST_TEST_EQ(e1 == e2, i1 == i2);
    BOOST_TEST_EQ(e1 != e2, i1 != i2);
    BOOST_TEST_EQ(e1 > e2, i1 > i2);
    BOOST_TEST_EQ(e1 >= e2, i1 >= i2);
    BOOST_TEST_EQ(e1 < e2, i1 < i2);
    BOOST_TEST_EQ(e1 <= e2, i1 <= i2);
  }
}

void extended_int_test6()
{
  typedef extended_int<1> eint32;
  eint32 e1(32);
  eint32 e2 = -e1;
  BOOST_TEST_EQ(e2.count(), -1);
  BOOST_TEST_EQ(e2.size(), 1U);
  BOOST_TEST_EQ(e2.chunks()[0], 32U);
}

void extended_int_test7()
{
  typedef extended_int<2> eint64;
  boost::mt19937_64 gen(static_cast<uint32>(time(NULL)));
  for (int i = 0; i < 1000; ++i) {
    int64 i1 = static_cast<int64>(gen()) >> 2;
    int64 i2 = static_cast<int64>(gen()) >> 2;
    eint64 e1(i1), e2(i2), e3(i1 + i2), e4(i1 - i2);
    BOOST_TEST(e1 + e2 == e3);
    BOOST_TEST(e1 - e2 == e4);
  }
}

void extended_int_test8()
{
  typedef extended_int<2> eint64;
  boost::mt19937 gen(static_cast<uint32>(time(NULL)));
  for (int i = 0; i < 1000; ++i) {
    int64 i1 = static_cast<int32>(gen());
    int64 i2 = static_cast<int32>(gen());
    eint64 e1(i1), e2(i2), e3(i1 * i2);
    BOOST_TEST(e1 * e2 == e3);
  }
}

void extended_int_test9()
{
  typedef extended_int<1> eint32;
  for (int i = -10; i <= 10; ++i) {
    for (int j = -10; j <= 10; ++j) {
      eint32 e1(i), e2(j), e3(i+j), e4(i-j), e5(i*j);
      BOOST_TEST(e1 + e2 == e3);
      BOOST_TEST(e1 - e2 == e4);
      BOOST_TEST(e1 * e2 == e5);
    }
  }
}

void extended_int_test10()
{
  typedef extended_int<2> eint64;
  boost::mt19937_64 gen(static_cast<uint32>(time(NULL)));
  for (int i = 0; i < 100; ++i) {
    int64 i1 = static_cast<int64>(gen()) >> 20;
    int64 i2 = i1 >> 32;
    eint64 e1(i1), e2(i2);
    BOOST_TEST(to_fpt(e1) == static_cast<fpt64>(i1));
    BOOST_TEST(to_fpt(e2) == static_cast<fpt64>(i2));
  }
}

void extened_int_test11()
{
  typedef extended_int<64> eint2048;
  eint2048 two(2), value(1);
  for (int i = 0; i < 1024; ++i)
    value = value * two;
  BOOST_TEST_EQ(value.count(), 33);
  for (std::size_t i = 1; i < value.size(); ++i)
    BOOST_TEST_EQ(value.chunks()[i-1], 0U);
  BOOST_TEST_EQ(value.chunks()[32], 1U);
}

int main()
{
    ulp_comparison_test1();
    ulp_comparison_test2();
    extended_exponent_fpt_test1();
    extended_exponent_fpt_test2();
    extended_exponent_fpt_test3();
    extended_exponent_fpt_test4();
    extended_exponent_fpt_test5();
    extended_exponent_fpt_test6();
    extended_int_test1();
    extended_int_test2();
    extended_int_test3();
    extended_int_test4();
    extended_int_test5();
    extended_int_test6();
    extended_int_test7();
    extended_int_test8();
    extended_int_test9();
    extended_int_test10();
    extened_int_test11();
    return boost::report_errors();
}
