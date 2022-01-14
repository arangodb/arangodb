//  (C) Copyright Raffi Enficiaud 2019.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example84
#include <boost/test/included/unit_test.hpp>
#include <random>
#include <cmath>

// this function does not compute properly the polynomial root estimation
// in the case of a double root.
template <class random_generator_t>
std::pair<double, double> estimate_polynomial_roots(
  random_generator_t& gen,
  std::function<double(double)> polynomial) {

  using namespace std;

  std::uniform_real_distribution<> dis(-10, 10);
  double x1 = dis(gen);
  double x2 = dis(gen);
  double fx1 = polynomial(x1);
  double fx2 = polynomial(x2);

  BOOST_TEST_INFO_SCOPE("sample1 = " << x1);
  BOOST_TEST_INFO_SCOPE("sample2 = " << x2);

  // from Vieta formula
  double minus_b = x2 + x1 - (fx2 - fx1) / (x2 - x1);
  double c = (x1 * fx2 - x2 * fx1 + x2 * x1 * x1 - x1 * x2 * x2) / (x1 - x2);

  BOOST_TEST(minus_b * minus_b >= 4*c);

  return std::make_pair(
    (minus_b - sqrt(minus_b * minus_b - 4 * c)) / 2,
    (minus_b + sqrt(minus_b * minus_b - 4 * c)) / 2);
}

BOOST_AUTO_TEST_CASE(quadratic_estimation)
{
  std::random_device rd;
  unsigned int seed = rd();
  std::mt19937 gen(seed);
  std::uniform_int_distribution<> dis(-10, 10);

  BOOST_TEST_MESSAGE("Seed = " << seed);

  for(int i = 0; i < 50; i++) {
    BOOST_TEST_INFO_SCOPE("trial " << i+1);
    int root1 = dis(gen);
    int root2 = dis(gen);
    if(root1 > root2) {
      std::swap(root1, root2);
    }
    BOOST_TEST_INFO_SCOPE("root1 = " << root1);
    BOOST_TEST_INFO_SCOPE("root2 = " << root2);

    std::pair<double, double> estimated = estimate_polynomial_roots(
      gen,
      [root1, root2](double x) -> double { return (x - root1) * (x - root2); });

    BOOST_TEST(estimated.first == double(root1), 10. % boost::test_tools::tolerance());
    BOOST_TEST(estimated.second == double(root2), 10. % boost::test_tools::tolerance());
  }
}
//]

BOOST_AUTO_TEST_CASE(making_it_fail)
{
  // cheating a bit ... but shhhh, do not show in the docs ...
  BOOST_FAIL("Making it fail always on all platforms");
}
