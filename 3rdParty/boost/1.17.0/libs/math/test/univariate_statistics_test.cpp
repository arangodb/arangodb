/*
 *  (C) Copyright Nick Thompson 2018.
 *  Use, modification and distribution are subject to the
 *  Boost Software License, Version 1.0. (See accompanying file
 *  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <vector>
#include <array>
#include <forward_list>
#include <algorithm>
#include <random>
#include <boost/core/lightweight_test.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/math/tools/univariate_statistics.hpp>
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

 // To stress test, set global_seed = 0, global_size = huge.
 static const constexpr size_t global_seed = 0;
 static const constexpr size_t global_size = 128;

template<class T>
std::vector<T> generate_random_vector(size_t size, size_t seed)
{
    if (seed == 0)
    {
        std::random_device rd;
        seed = rd();
    }
    std::vector<T> v(size);

    std::mt19937 gen(seed);

    if constexpr (std::is_floating_point<T>::value)
    {
        std::normal_distribution<T> dis(0, 1);
        for (size_t i = 0; i < v.size(); ++i)
        {
         v[i] = dis(gen);
        }
        return v;
    }
    else if constexpr (std::is_integral<T>::value)
    {
        // Rescaling by larger than 2 is UB!
        std::uniform_int_distribution<T> dis(std::numeric_limits<T>::lowest()/2, std::numeric_limits<T>::max()/2);
        for (size_t i = 0; i < v.size(); ++i)
        {
         v[i] = dis(gen);
        }
        return v;
    }
    else if constexpr (boost::is_complex<T>::value)
    {
        std::normal_distribution<typename T::value_type> dis(0, 1);
        for (size_t i = 0; i < v.size(); ++i)
        {
            v[i] = {dis(gen), dis(gen)};
        }
        return v;
    }
    else if constexpr (boost::multiprecision::number_category<T>::value == boost::multiprecision::number_kind_complex)
    {
        std::normal_distribution<long double> dis(0, 1);
        for (size_t i = 0; i < v.size(); ++i)
        {
            v[i] = {dis(gen), dis(gen)};
        }
        return v;
    }
    else if constexpr (boost::multiprecision::number_category<T>::value == boost::multiprecision::number_kind_floating_point)
    {
        std::normal_distribution<long double> dis(0, 1);
        for (size_t i = 0; i < v.size(); ++i)
        {
            v[i] = dis(gen);
        }
        return v;
    }
    else
    {
        BOOST_ASSERT_MSG(false, "Could not identify type for random vector generation.");
        return v;
    }
}


template<class Z>
void test_integer_mean()
{
    double tol = 100*std::numeric_limits<double>::epsilon();
    std::vector<Z> v{1,2,3,4,5};
    double mu = boost::math::tools::mean(v);
    BOOST_TEST(abs(mu - 3) < tol);

    // Work with std::array?
    std::array<Z, 5> w{1,2,3,4,5};
    mu = boost::math::tools::mean(w);
    BOOST_TEST(abs(mu - 3) < tol);

    v = generate_random_vector<Z>(global_size, global_seed);
    Z scale = 2;

    double m1 = scale*boost::math::tools::mean(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    double m2 = boost::math::tools::mean(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));
}

template<class RandomAccessContainer>
auto naive_mean(RandomAccessContainer const & v)
{
    typename RandomAccessContainer::value_type sum = 0;
    for (auto & x : v)
    {
        sum += x;
    }
    return sum/v.size();
}

template<class Real>
void test_mean()
{
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,2,3,4,5};
    Real mu = boost::math::tools::mean(v.begin(), v.end());
    BOOST_TEST(abs(mu - 3) < tol);

    // Does range call work?
    mu = boost::math::tools::mean(v);
    BOOST_TEST(abs(mu - 3) < tol);

    // Can we successfully average only part of the vector?
    mu = boost::math::tools::mean(v.begin(), v.begin() + 3);
    BOOST_TEST(abs(mu - 2) < tol);

    // Does it work when we const qualify?
    mu = boost::math::tools::mean(v.cbegin(), v.cend());
    BOOST_TEST(abs(mu - 3) < tol);

    // Does it work for std::array?
    std::array<Real, 7> u{1,2,3,4,5,6,7};
    mu = boost::math::tools::mean(u.begin(), u.end());
    BOOST_TEST(abs(mu - 4) < tol);

    // Does it work for a forward iterator?
    std::forward_list<Real> l{1,2,3,4,5,6,7};
    mu = boost::math::tools::mean(l.begin(), l.end());
    BOOST_TEST(abs(mu - 4) < tol);

    // Does it work with ublas vectors?
    boost::numeric::ublas::vector<Real> w(7);
    for (size_t i = 0; i < w.size(); ++i)
    {
        w[i] = i+1;
    }
    mu = boost::math::tools::mean(w.cbegin(), w.cend());
    BOOST_TEST(abs(mu - 4) < tol);

    v = generate_random_vector<Real>(global_size, global_seed);
    Real scale = 2;
    Real m1 = scale*boost::math::tools::mean(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    Real m2 = boost::math::tools::mean(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));

    // Stress test:
    for (size_t i = 1; i < 30; ++i)
    {
        v = generate_random_vector<Real>(i, 12803);
        auto naive_ = naive_mean(v);
        auto higham_ = boost::math::tools::mean(v);
        if (abs(higham_ - naive_) >= 100*tol*abs(naive_))
        {
            std::cout << std::hexfloat;
            std::cout << "Terms = " << v.size() << "\n";
            std::cout << "higham = " << higham_ << "\n";
            std::cout << "naive_ = " << naive_ << "\n";
        }
        BOOST_TEST(abs(higham_ - naive_) < 100*tol*abs(naive_));
    }

}

template<class Complex>
void test_complex_mean()
{
    typedef typename Complex::value_type Real;
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Complex> v{{0,1},{0,2},{0,3},{0,4},{0,5}};
    auto mu = boost::math::tools::mean(v.begin(), v.end());
    BOOST_TEST(abs(mu.imag() - 3) < tol);
    BOOST_TEST(abs(mu.real()) < tol);

    // Does range work?
    mu = boost::math::tools::mean(v);
    BOOST_TEST(abs(mu.imag() - 3) < tol);
    BOOST_TEST(abs(mu.real()) < tol);
}

template<class Real>
void test_variance()
{
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1,1,1,1};
    Real sigma_sq = boost::math::tools::variance(v.begin(), v.end());
    BOOST_TEST(abs(sigma_sq) < tol);

    sigma_sq = boost::math::tools::variance(v);
    BOOST_TEST(abs(sigma_sq) < tol);

    Real s_sq = boost::math::tools::sample_variance(v);
    BOOST_TEST(abs(s_sq) < tol);

    std::vector<Real> u{1};
    sigma_sq = boost::math::tools::variance(u.cbegin(), u.cend());
    BOOST_TEST(abs(sigma_sq) < tol);

    std::array<Real, 8> w{0,1,0,1,0,1,0,1};
    sigma_sq = boost::math::tools::variance(w.begin(), w.end());
    BOOST_TEST(abs(sigma_sq - 1.0/4.0) < tol);

    sigma_sq = boost::math::tools::variance(w);
    BOOST_TEST(abs(sigma_sq - 1.0/4.0) < tol);

    std::forward_list<Real> l{0,1,0,1,0,1,0,1};
    sigma_sq = boost::math::tools::variance(l.begin(), l.end());
    BOOST_TEST(abs(sigma_sq - 1.0/4.0) < tol);

    v = generate_random_vector<Real>(global_size, global_seed);
    Real scale = 2;
    Real m1 = scale*scale*boost::math::tools::variance(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    Real m2 = boost::math::tools::variance(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));

    // Wikipedia example for a variance of N sided die:
    // https://en.wikipedia.org/wiki/Variance
    for (size_t j = 16; j < 2048; j *= 2)
    {
        v.resize(1024);
        Real n = v.size();
        for (size_t i = 0; i < v.size(); ++i)
        {
            v[i] = i + 1;
        }

        sigma_sq = boost::math::tools::variance(v);

        BOOST_TEST(abs(sigma_sq - (n*n-1)/Real(12)) <= tol*sigma_sq);
    }

}

template<class Z>
void test_integer_variance()
{
    double tol = std::numeric_limits<double>::epsilon();
    std::vector<Z> v{1,1,1,1,1,1};
    double sigma_sq = boost::math::tools::variance(v);
    BOOST_TEST(abs(sigma_sq) < tol);

    std::forward_list<Z> l{0,1,0,1,0,1,0,1};
    sigma_sq = boost::math::tools::variance(l.begin(), l.end());
    BOOST_TEST(abs(sigma_sq - 1.0/4.0) < tol);

    v = generate_random_vector<Z>(global_size, global_seed);
    Z scale = 2;
    double m1 = scale*scale*boost::math::tools::variance(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    double m2 = boost::math::tools::variance(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));
}

template<class Z>
void test_integer_skewness()
{
    double tol = std::numeric_limits<double>::epsilon();
    std::vector<Z> v{1,1,1};
    double skew = boost::math::tools::skewness(v);
    BOOST_TEST(abs(skew) < tol);

    // Dataset is symmetric about the mean:
    v = {1,2,3,4,5};
    skew = boost::math::tools::skewness(v);
    BOOST_TEST(abs(skew) < tol);

    v = {0,0,0,0,5};
    // mu = 1, sigma^2 = 4, sigma = 2, skew = 3/2
    skew = boost::math::tools::skewness(v);
    BOOST_TEST(abs(skew - 3.0/2.0) < tol);

    std::forward_list<Z> v2{0,0,0,0,5};
    skew = boost::math::tools::skewness(v);
    BOOST_TEST(abs(skew - 3.0/2.0) < tol);


    v = generate_random_vector<Z>(global_size, global_seed);
    Z scale = 2;
    double m1 = boost::math::tools::skewness(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    double m2 = boost::math::tools::skewness(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));

}

template<class Real>
void test_skewness()
{
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1};
    Real skew = boost::math::tools::skewness(v);
    BOOST_TEST(abs(skew) < tol);

    // Dataset is symmetric about the mean:
    v = {1,2,3,4,5};
    skew = boost::math::tools::skewness(v);
    BOOST_TEST(abs(skew) < tol);

    v = {0,0,0,0,5};
    // mu = 1, sigma^2 = 4, sigma = 2, skew = 3/2
    skew = boost::math::tools::skewness(v);
    BOOST_TEST(abs(skew - Real(3)/Real(2)) < tol);

    std::array<Real, 5> w1{0,0,0,0,5};
    skew = boost::math::tools::skewness(w1);
    BOOST_TEST(abs(skew - Real(3)/Real(2)) < tol);

    std::forward_list<Real> w2{0,0,0,0,5};
    skew = boost::math::tools::skewness(w2);
    BOOST_TEST(abs(skew - Real(3)/Real(2)) < tol);

    v = generate_random_vector<Real>(global_size, global_seed);
    Real scale = 2;
    Real m1 = boost::math::tools::skewness(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    Real m2 = boost::math::tools::skewness(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));
}

template<class Real>
void test_kurtosis()
{
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1};
    Real kurt = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(kurt) < tol);

    v = {1,2,3,4,5};
    // mu =1, sigma^2 = 2, kurtosis = 17/10
    kurt = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(kurt - Real(17)/Real(10)) < tol);

    v = {0,0,0,0,5};
    // mu = 1, sigma^2 = 4, sigma = 2, skew = 3/2, kurtosis = 13/4
    kurt = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(kurt - Real(13)/Real(4)) < tol);

    std::array<Real, 5> v1{0,0,0,0,5};
    kurt = boost::math::tools::kurtosis(v1);
    BOOST_TEST(abs(kurt - Real(13)/Real(4)) < tol);

    std::forward_list<Real> v2{0,0,0,0,5};
    kurt = boost::math::tools::kurtosis(v2);
    BOOST_TEST(abs(kurt - Real(13)/Real(4)) < tol);

    std::vector<Real> v3(10000);
    std::mt19937 gen(42);
    std::normal_distribution<long double> dis(0, 1);
    for (size_t i = 0; i < v3.size(); ++i) {
        v3[i] = dis(gen);
    }
    kurt = boost::math::tools::kurtosis(v3);
    BOOST_TEST(abs(kurt - 3) < 0.1);

    std::uniform_real_distribution<long double> udis(-1, 3);
    for (size_t i = 0; i < v3.size(); ++i) {
        v3[i] = udis(gen);
    }
    auto excess_kurtosis = boost::math::tools::excess_kurtosis(v3);
    BOOST_TEST(abs(excess_kurtosis + 6.0/5.0) < 0.2);

    v = generate_random_vector<Real>(global_size, global_seed);
    Real scale = 2;
    Real m1 = boost::math::tools::kurtosis(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    Real m2 = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));

    // This test only passes when there are a large number of samples.
    // Otherwise, the distribution doesn't generate enough outliers to give,
    // or generates too many, giving pretty wildly different values of kurtosis on different runs.
    // However, by kicking up the samples to 1,000,000, I got very close to 6 for the excess kurtosis on every run.
    // The CI system, however, would die on a million long doubles.
    //v3.resize(1000000);
    //std::exponential_distribution<long double> edis(0.1);
    //for (size_t i = 0; i < v3.size(); ++i) {
    //    v3[i] = edis(gen);
    //}
    //excess_kurtosis = boost::math::tools::kurtosis(v3) - 3;
    //BOOST_TEST(abs(excess_kurtosis - 6.0) < 0.2);
}

template<class Z>
void test_integer_kurtosis()
{
    double tol = std::numeric_limits<double>::epsilon();
    std::vector<Z> v{1,1,1};
    double kurt = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(kurt) < tol);

    v = {1,2,3,4,5};
    // mu =1, sigma^2 = 2, kurtosis = 17/10
    kurt = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(kurt - 17.0/10.0) < tol);

    v = {0,0,0,0,5};
    // mu = 1, sigma^2 = 4, sigma = 2, skew = 3/2, kurtosis = 13/4
    kurt = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(kurt - 13.0/4.0) < tol);

    v = generate_random_vector<Z>(global_size, global_seed);
    Z scale = 2;
    double m1 = boost::math::tools::kurtosis(v);
    for (auto & x : v)
    {
        x *= scale;
    }
    double m2 = boost::math::tools::kurtosis(v);
    BOOST_TEST(abs(m1 - m2) < tol*abs(m1));
}

template<class Real>
void test_first_four_moments()
{
    Real tol = 10*std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1};
    auto [M1_1, M2_1, M3_1, M4_1] = boost::math::tools::first_four_moments(v);
    BOOST_TEST(abs(M1_1 - 1) < tol);
    BOOST_TEST(abs(M2_1) < tol);
    BOOST_TEST(abs(M3_1) < tol);
    BOOST_TEST(abs(M4_1) < tol);

    v = {1, 2, 3, 4, 5};
    auto [M1_2, M2_2, M3_2, M4_2] = boost::math::tools::first_four_moments(v);
    BOOST_TEST(abs(M1_2 - 3) < tol);
    BOOST_TEST(abs(M2_2 - 2) < tol);
    BOOST_TEST(abs(M3_2) < tol);
    BOOST_TEST(abs(M4_2 - Real(34)/Real(5)) < tol);
}

template<class Real>
void test_median()
{
    std::mt19937 g(12);
    std::vector<Real> v{1,2,3,4,5,6,7};

    Real m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 4);

    std::shuffle(v.begin(), v.end(), g);
    // Does range call work?
    m = boost::math::tools::median(v);
    BOOST_TEST_EQ(m, 4);

    v = {1,2,3,3,4,5};
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 3);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 3);

    v = {1};
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    v = {1,1};
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    v = {2,4};
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 3);

    v = {1,1,1};
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    v = {1,2,3};
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 2);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::tools::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 2);

    // Does it work with std::array?
    std::array<Real, 3> w{1,2,3};
    m = boost::math::tools::median(w);
    BOOST_TEST_EQ(m, 2);

    // Does it work with ublas?
    boost::numeric::ublas::vector<Real> w1(3);
    w1[0] = 1;
    w1[1] = 2;
    w1[2] = 3;
    m = boost::math::tools::median(w);
    BOOST_TEST_EQ(m, 2);
}

template<class Real>
void test_median_absolute_deviation()
{
    std::vector<Real> v{-1, 2, -3, 4, -5, 6, -7};

    Real m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 4);

    std::mt19937 g(12);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::tools::median_absolute_deviation(v, 0);
    BOOST_TEST_EQ(m, 4);

    v = {1, -2, -3, 3, -4, -5};
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 3);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 3);

    v = {-1};
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 1);

    v = {-1, 1};
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 1);
    // The median is zero, so coincides with the default:
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    m = boost::math::tools::median_absolute_deviation(v);
    BOOST_TEST_EQ(m, 1);


    v = {2, -4};
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 3);

    v = {1, -1, 1};
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 1);

    v = {1, 2, -3};
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 2);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::tools::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 2);

    std::array<Real, 3> w{1, 2, -3};
    m = boost::math::tools::median_absolute_deviation(w, 0);
    BOOST_TEST_EQ(m, 2);

    // boost.ublas vector?
    boost::numeric::ublas::vector<Real> u(6);
    u[0] = 1;
    u[1] = 2;
    u[2] = -3;
    u[3] = 1;
    u[4] = 2;
    u[5] = -3;
    m = boost::math::tools::median_absolute_deviation(u, 0);
    BOOST_TEST_EQ(m, 2);
}


template<class Real>
void test_sample_gini_coefficient()
{
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,0,0};
    Real gini = boost::math::tools::sample_gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini - 1) < tol);

    gini = boost::math::tools::sample_gini_coefficient(v);
    BOOST_TEST(abs(gini - 1) < tol);

    v[0] = 1;
    v[1] = 1;
    v[2] = 1;
    gini = boost::math::tools::sample_gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
    gini = boost::math::tools::sample_gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    std::array<Real, 3> w{0,0,0};
    gini = boost::math::tools::sample_gini_coefficient(w);
    BOOST_TEST(abs(gini) < tol);
}


template<class Real>
void test_gini_coefficient()
{
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,0,0};
    Real gini = boost::math::tools::gini_coefficient(v.begin(), v.end());
    Real expected = Real(2)/Real(3);
    BOOST_TEST(abs(gini - expected) < tol);

    gini = boost::math::tools::gini_coefficient(v);
    BOOST_TEST(abs(gini - expected) < tol);

    v[0] = 1;
    v[1] = 1;
    v[2] = 1;
    gini = boost::math::tools::gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
    gini = boost::math::tools::gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    std::array<Real, 3> w{0,0,0};
    gini = boost::math::tools::gini_coefficient(w);
    BOOST_TEST(abs(gini) < tol);

    boost::numeric::ublas::vector<Real> w1(3);
    w1[0] = 1;
    w1[1] = 1;
    w1[2] = 1;
    gini = boost::math::tools::gini_coefficient(w1);
    BOOST_TEST(abs(gini) < tol);

    std::mt19937 gen(18);
    // Gini coefficient for a uniform distribution is (b-a)/(3*(b+a));
    std::uniform_real_distribution<long double> dis(0, 3);
    expected = (dis.b() - dis.a())/(3*(dis.b()+ dis.a()));
    v.resize(1024);
    for(size_t i = 0; i < v.size(); ++i)
    {
        v[i] = dis(gen);
    }
    gini = boost::math::tools::gini_coefficient(v);
    BOOST_TEST(abs(gini - expected) < 0.01);

}

template<class Z>
void test_integer_gini_coefficient()
{
    double tol = std::numeric_limits<double>::epsilon();
    std::vector<Z> v{1,0,0};
    double gini = boost::math::tools::gini_coefficient(v.begin(), v.end());
    double expected = 2.0/3.0;
    BOOST_TEST(abs(gini - expected) < tol);

    gini = boost::math::tools::gini_coefficient(v);
    BOOST_TEST(abs(gini - expected) < tol);

    v[0] = 1;
    v[1] = 1;
    v[2] = 1;
    gini = boost::math::tools::gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
    gini = boost::math::tools::gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    std::array<Z, 3> w{0,0,0};
    gini = boost::math::tools::gini_coefficient(w);
    BOOST_TEST(abs(gini) < tol);

    boost::numeric::ublas::vector<Z> w1(3);
    w1[0] = 1;
    w1[1] = 1;
    w1[2] = 1;
    gini = boost::math::tools::gini_coefficient(w1);
    BOOST_TEST(abs(gini) < tol);
}

int main()
{
    test_mean<float>();
    test_mean<double>();
    test_mean<long double>();
    test_mean<cpp_bin_float_50>();

    test_integer_mean<unsigned>();
    test_integer_mean<int>();

    test_complex_mean<std::complex<float>>();
    test_complex_mean<cpp_complex_50>();

    test_variance<float>();
    test_variance<double>();
    test_variance<long double>();
    test_variance<cpp_bin_float_50>();

    test_integer_variance<int>();
    test_integer_variance<unsigned>();

    test_skewness<float>();
    test_skewness<double>();
    test_skewness<long double>();
    test_skewness<cpp_bin_float_50>();

    test_integer_skewness<int>();
    test_integer_skewness<unsigned>();

    test_first_four_moments<float>();
    test_first_four_moments<double>();
    test_first_four_moments<long double>();
    test_first_four_moments<cpp_bin_float_50>();

    test_kurtosis<float>();
    test_kurtosis<double>();
    test_kurtosis<long double>();
    // Kinda expensive:
    //test_kurtosis<cpp_bin_float_50>();

    test_integer_kurtosis<int>();
    test_integer_kurtosis<unsigned>();

    test_median<float>();
    test_median<double>();
    test_median<long double>();
    test_median<cpp_bin_float_50>();
    test_median<int>();

    test_median_absolute_deviation<float>();
    test_median_absolute_deviation<double>();
    test_median_absolute_deviation<long double>();
    test_median_absolute_deviation<cpp_bin_float_50>();

    test_gini_coefficient<float>();
    test_gini_coefficient<double>();
    test_gini_coefficient<long double>();
    test_gini_coefficient<cpp_bin_float_50>();

    test_integer_gini_coefficient<unsigned>();
    test_integer_gini_coefficient<int>();

    test_sample_gini_coefficient<float>();
    test_sample_gini_coefficient<double>();
    test_sample_gini_coefficient<long double>();
    test_sample_gini_coefficient<cpp_bin_float_50>();

    return boost::report_errors();
}
