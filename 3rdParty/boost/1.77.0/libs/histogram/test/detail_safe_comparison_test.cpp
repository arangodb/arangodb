// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/detail/safe_comparison.hpp>

using namespace boost::histogram::detail;

int main() {
  auto eq = safe_equal{};
  BOOST_TEST(eq(-1, -1));
  BOOST_TEST(eq(1, 1u));
  BOOST_TEST(eq(1u, 1));
  BOOST_TEST(eq(1u, 1u));
  BOOST_TEST(eq(1.0, 1));
  BOOST_TEST(eq(1, 1.0));
  BOOST_TEST(eq(1.0, 1u));
  BOOST_TEST(eq(1u, 1.0));
  BOOST_TEST_NOT(eq(-1, static_cast<unsigned>(-1)));
  BOOST_TEST_NOT(eq(static_cast<unsigned>(-1), -1));

  auto lt = safe_less{};
  BOOST_TEST(lt(1u, 2u));
  BOOST_TEST(lt(-1, 1u));
  BOOST_TEST(lt(1u, 2));
  BOOST_TEST(lt(-2, -1));
  BOOST_TEST(lt(-2.0, -1));
  BOOST_TEST(lt(1, 2.0));
  BOOST_TEST(lt(-1.0, 1u));
  BOOST_TEST(lt(1u, 2.0));
  BOOST_TEST(lt(1.0, 2.0));
  BOOST_TEST_NOT(lt(1u, 1));
  BOOST_TEST_NOT(lt(1, 1u));
  BOOST_TEST_NOT(lt(1.0, 1));
  BOOST_TEST_NOT(lt(1, 1.0));
  BOOST_TEST_NOT(lt(1.0, 1u));
  BOOST_TEST_NOT(lt(1u, 1.0));
  BOOST_TEST_NOT(lt(1.0, 1.0));

  auto gt = safe_greater{};
  BOOST_TEST(gt(2u, 1u));
  BOOST_TEST(gt(1u, -1));
  BOOST_TEST(gt(2, 1u));
  BOOST_TEST(gt(-1, -2));
  BOOST_TEST(gt(-1, -2.0));
  BOOST_TEST(gt(2.0, 1));
  BOOST_TEST(gt(1u, -1.0));
  BOOST_TEST(gt(2.0, 1u));
  BOOST_TEST_NOT(gt(1u, 1));
  BOOST_TEST_NOT(gt(1, 1u));
  BOOST_TEST_NOT(gt(1.0, 1));
  BOOST_TEST_NOT(gt(1, 1.0));
  BOOST_TEST_NOT(gt(1.0, 1u));
  BOOST_TEST_NOT(gt(1u, 1.0));

  return boost::report_errors();
}
