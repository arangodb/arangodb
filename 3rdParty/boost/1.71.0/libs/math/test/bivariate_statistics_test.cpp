/*
 *  (C) Copyright Nick Thompson 2018.
 *  Use, modification and distribution are subject to the
 *  Boost Software License, Version 1.0. (See accompanying file
 *  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <forward_list>
#include <algorithm>
#include <random>
#include <boost/core/lightweight_test.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/math/tools/univariate_statistics.hpp>
#include <boost/math/tools/bivariate_statistics.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_complex.hpp>

using boost::multiprecision::cpp_bin_float_50;
using boost::multiprecision::cpp_complex_50;

/*
 * Test checklist:
 * 1) Does it work with multiprecision?
 * 2) Does it work with .cbegin()/.cend() if the data is not altered?
 * 3) Does it work with ublas and std::array? (Checking Eigen and Armadillo will make the CI system really unhappy.)
 * 4) Does it work with std::forward_list if a forward iterator is all that is required?
 * 5) Does it work with complex data if complex data is sensible?
 */

using  boost::math::tools::means_and_covariance;
using  boost::math::tools::covariance;

template<class Real>
void test_covariance()
{
    std::cout << std::setprecision(std::numeric_limits<Real>::digits10+1);
    Real tol = std::numeric_limits<Real>::epsilon();
    using std::abs;

    // Covariance of a single thing is zero:
    std::array<Real, 1> u1{8};
    std::array<Real, 1> v1{17};
    auto [mu_u1, mu_v1, cov1] = means_and_covariance(u1, v1);

    BOOST_TEST(abs(cov1) < tol);
    BOOST_TEST(abs(mu_u1 - 8) < tol);
    BOOST_TEST(abs(mu_v1 - 17) < tol);


    std::array<Real, 2> u2{8, 4};
    std::array<Real, 2> v2{3, 7};
    auto [mu_u2, mu_v2, cov2] = means_and_covariance(u2, v2);

    BOOST_TEST(abs(cov2+4) < tol);
    BOOST_TEST(abs(mu_u2 - 6) < tol);
    BOOST_TEST(abs(mu_v2 - 5) < tol);

    std::vector<Real> u3{1,2,3};
    std::vector<Real> v3{1,1,1};

    auto [mu_u3, mu_v3, cov3] = means_and_covariance(u3, v3);

    // Since v is constant, covariance(u,v) = 0 against everything any u:
    BOOST_TEST(abs(cov3) < tol);
    BOOST_TEST(abs(mu_u3 - 2) < tol);
    BOOST_TEST(abs(mu_v3 - 1) < tol);
    // Make sure we pull the correct symbol out of means_and_covariance:
    cov3 = covariance(u3, v3);
    BOOST_TEST(abs(cov3) < tol);

    cov3 = covariance(v3, u3);
    // Covariance is symmetric: cov(u,v) = cov(v,u)
    BOOST_TEST(abs(cov3) < tol);

    // cov(u,u) = sigma(u)^2:
    cov3 = covariance(u3, u3);
    Real expected = Real(2)/Real(3);

    BOOST_TEST(abs(cov3 - expected) < tol);

    std::mt19937 gen(15);
    // Can't template standard library on multiprecision, so use double and cast back:
    std::uniform_real_distribution<double> dis(-1.0, 1.0);
    std::vector<Real> u(500);
    std::vector<Real> v(500);
    for(size_t i = 0; i < u.size(); ++i)
    {
        u[i] = (Real) dis(gen);
        v[i] = (Real) dis(gen);
    }

    Real mu_u = boost::math::tools::mean(u);
    Real mu_v = boost::math::tools::mean(v);
    Real sigma_u_sq = boost::math::tools::variance(u);
    Real sigma_v_sq = boost::math::tools::variance(v);

    auto [mu_u_, mu_v_, cov_uv] = means_and_covariance(u, v);
    BOOST_TEST(abs(mu_u - mu_u_) < tol);
    BOOST_TEST(abs(mu_v - mu_v_) < tol);

    // Cauchy-Schwartz inequality:
    BOOST_TEST(cov_uv*cov_uv <= sigma_u_sq*sigma_v_sq);
    // cov(X, X) = sigma(X)^2:
    Real cov_uu = covariance(u, u);
    BOOST_TEST(abs(cov_uu - sigma_u_sq) < tol);
    Real cov_vv = covariance(v, v);
    BOOST_TEST(abs(cov_vv - sigma_v_sq) < tol);

}

template<class Real>
void test_correlation_coefficient()
{
    using boost::math::tools::correlation_coefficient;

    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> u{1};
    std::vector<Real> v{1};
    Real rho_uv = correlation_coefficient(u, v);
    BOOST_TEST(abs(rho_uv - 1) < tol);

    u = {1,1};
    v = {1,1};
    rho_uv = correlation_coefficient(u, v);
    BOOST_TEST(abs(rho_uv - 1) < tol);

    u = {1, 2, 3};
    v = {1, 2, 3};
    rho_uv = correlation_coefficient(u, v);
    BOOST_TEST(abs(rho_uv - 1) < tol);

    u = {1, 2, 3};
    v = {-1, -2, -3};
    rho_uv = correlation_coefficient(u, v);
    BOOST_TEST(abs(rho_uv + 1) < tol);

    rho_uv = correlation_coefficient(v, u);
    BOOST_TEST(abs(rho_uv + 1) < tol);

    u = {1, 2, 3};
    v = {0, 0, 0};
    rho_uv = correlation_coefficient(v, u);
    BOOST_TEST(abs(rho_uv) < tol);

    u = {1, 2, 3};
    v = {0, 0, 3};
    rho_uv = correlation_coefficient(v, u);
    // mu_u = 2, sigma_u^2 = 2/3, mu_v = 1, sigma_v^2 = 2, cov(u,v) = 1.
    BOOST_TEST(abs(rho_uv - sqrt(Real(3))/Real(2)) < tol);
}

int main()
{
    test_covariance<float>();
    test_covariance<double>();
    test_covariance<long double>();
    test_covariance<cpp_bin_float_50>();

    test_correlation_coefficient<float>();
    test_correlation_coefficient<double>();
    test_correlation_coefficient<long double>();
    test_correlation_coefficient<cpp_bin_float_50>();

    return boost::report_errors();
}
