///////////////////////////////////////////////////////////////////////////////
//      Copyright Christopher Kormanyos 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//

// This example exercises Boost.Multiprecision in concurrent
// multi-threaded environments. To do so a loop involving
// non-trivial calculations of numerous function values
// has been set up within both concurrent as well as
// sequential running environments. In particular,
// this example uses an AGM method to do a "from the ground up"
// calculation of logarithms. The logarithm functions values
// are compared with the values from Boost.Multiprecision's
// specific log functions for the relevant backends.
// The log GM here is not optimized or intended for
// high-performance work, but can be taken as an
// interesting example of an AGM iteration if helpful.

// This example has been initially motivated in part
// by discussions in:
// https://github.com/boostorg/multiprecision/pull/211

// We find the following performance data here:
// https://github.com/boostorg/multiprecision/pull/213
//
// cpp_dec_float:
// result_is_ok_concurrent: true, calculation_time_concurrent: 18.1s
// result_is_ok_sequential: true, calculation_time_sequential: 48.5s
//
// cpp_bin_float:
// result_is_ok_concurrent: true, calculation_time_concurrent: 18.7s
// result_is_ok_sequential: true, calculation_time_sequential: 50.4s
//
// gmp_float:
// result_is_ok_concurrent: true, calculation_time_concurrent: 3.3s
// result_is_ok_sequential: true, calculation_time_sequential: 12.4s
//
// mpfr_float:
// result_is_ok_concurrent: true, calculation_time_concurrent: 0.6s
// result_is_ok_sequential: true, calculation_time_sequential: 1.9s

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/prime.hpp>

#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_CPP_DEC_FLOAT       101
#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_GMP_FLOAT           102
#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_CPP_BIN_FLOAT       103
#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_MPFR_FLOAT          104

#if !defined(BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE)
#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_CPP_DEC_FLOAT
//#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_CPP_BIN_FLOAT
//#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_GMP_FLOAT
//#define BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_MPFR_FLOAT
#endif

#if  (BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE == BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_CPP_DEC_FLOAT)
#include <boost/multiprecision/cpp_dec_float.hpp>

using big_float_type = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<501>,
                                                     boost::multiprecision::et_off>;

#elif (BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE == BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_CPP_BIN_FLOAT)
#include <boost/multiprecision/cpp_bin_float.hpp>

using big_float_type = boost::multiprecision::number<boost::multiprecision::cpp_bin_float<501>,
                                                     boost::multiprecision::et_off>;

#elif  (BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE == BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_GMP_FLOAT)
#include <boost/multiprecision/gmp.hpp>

using big_float_type = boost::multiprecision::number<boost::multiprecision::gmp_float<501>,
                                                     boost::multiprecision::et_off>;

#elif  (BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE == BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_MPFR_FLOAT)
#include <boost/multiprecision/mpfr.hpp>

using big_float_type = boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<501>,
                                                     boost::multiprecision::et_off>;

#else
#error BOOST_MULTIPRECISION_EXERCISE_THREADING_BACKEND_TYPE is undefined.
#endif

namespace boost { namespace multiprecision { namespace exercise_threading {

namespace detail {

namespace my_concurrency {
template<typename index_type,
         typename callable_function_type>
void parallel_for(index_type             start,
                  index_type             end,
                  callable_function_type parallel_function)
{
  // Estimate the number of threads available.
  static const unsigned int number_of_threads_hint =
    std::thread::hardware_concurrency();

  static const unsigned int number_of_threads_total =
    ((number_of_threads_hint == 0U) ? 4U : number_of_threads_hint);

  // Use only 3/4 of the available cores.
  static const unsigned int number_of_threads = number_of_threads_total - (number_of_threads_total / 8U);

  std::cout << "Executing with " << number_of_threads << " threads" << std::endl;

  // Set the size of a slice for the range functions.
  index_type n = index_type(end - start) + index_type(1);

  index_type slice =
    static_cast<index_type>(std::round(n / static_cast<float>(number_of_threads)));

  slice = (std::max)(slice, index_type(1));

  // Inner loop.
  auto launch_range =
    [&parallel_function](index_type index_lo, index_type index_hi)
    {
      for(index_type i = index_lo; i < index_hi; ++i)
      {
        parallel_function(i);
      }
    };

  // Create the thread pool and launch the jobs.
  std::vector<std::thread> pool;

  pool.reserve(number_of_threads);

  index_type i1 = start;
  index_type i2 = (std::min)(index_type(start + slice), end);

  for(index_type i = 0U; ((index_type(i + index_type(1U)) < number_of_threads) && (i1 < end)); ++i)
  {
    pool.emplace_back(launch_range, i1, i2);

    i1 = i2;

    i2 = (std::min)(index_type(i2 + slice), end);
  }

  if(i1 < end)
  {
    pool.emplace_back(launch_range, i1, end);
  }

  // Wait for the jobs to finish.
  for(std::thread& thread_in_pool : pool)
  {
    if(thread_in_pool.joinable())
    {
      thread_in_pool.join();
    }
  }
}
} // namespace my_concurrency

template<typename FloatingPointType,
         typename UnsignedIntegralType>
FloatingPointType pown(const FloatingPointType& b, const UnsignedIntegralType& p)
{
  // Calculate (b ^ p).

  using local_floating_point_type    = FloatingPointType;
  using local_unsigned_integral_type = UnsignedIntegralType;

  local_floating_point_type result;

  if     (p == local_unsigned_integral_type(0U)) { result = local_floating_point_type(1U); }
  else if(p == local_unsigned_integral_type(1U)) { result = b; }
  else if(p == local_unsigned_integral_type(2U)) { result = b; result *= b; }
  else
  {
    result = local_floating_point_type(1U);

    local_floating_point_type y(b);

    for(local_unsigned_integral_type p_local(p); p_local != local_unsigned_integral_type(0U); p_local >>= 1U)
    {
      if((static_cast<unsigned>(p_local) & 1U) != 0U)
      {
        result *= y;
      }

      y *= y;
    }
  }

  return result;
}

const std::vector<std::uint32_t>& primes()
{
  static std::vector<std::uint32_t> my_primes;

  if(my_primes.empty())
  {
    my_primes.resize(10000U);

    // Get exactly 10,000 primes.
    for(std::size_t i = 0U; i < my_primes.size(); ++i)
    {
      my_primes[i] = boost::math::prime((unsigned int) i);
    }
  }

  return my_primes;
}

} // namespace detail

template<typename FloatingPointType>
FloatingPointType log(const FloatingPointType& x)
{
  // Use an AGM method to compute the logarithm of x.

  // For values less than 1 invert the argument and
  // remember (in this case) to negate the result below.
  const bool b_negate = (x < 1);

  const FloatingPointType xx = ((b_negate == false) ? x : 1 / x);

  // Set a0 = 1
  // Set b0 = 4 / (x * 2^m) = 1 / (x * 2^(m - 2))

  FloatingPointType ak(1U);

  const float n_times_factor = static_cast<float>(static_cast<float>(std::numeric_limits<FloatingPointType>::digits10) * 1.67F);
  const float lgx_over_lg2   = std::log(static_cast<float>(xx)) / std::log(2.0F);

  std::int32_t m = static_cast<std::int32_t>(n_times_factor - lgx_over_lg2);

  // Ensure that the resulting power is non-negative.
  // Also enforce that m >= 8.
  m = (std::max)(m, static_cast<std::int32_t>(8));

  FloatingPointType bk = detail::pown(FloatingPointType(2), static_cast<std::uint32_t>(m));

  bk *= xx;
  bk  = 4 / bk;

  FloatingPointType ak_tmp(0U);

  using std::sqrt;

  // Determine the requested precision of the upcoming iteration in units of digits10.
  const FloatingPointType target_tolerance = sqrt(std::numeric_limits<FloatingPointType>::epsilon()) / 100;

  for(std::int32_t k = static_cast<std::int32_t>(0); k < static_cast<std::int32_t>(64); ++k)
  {
    using std::fabs;

    // Check for the number of significant digits to be
    // at least half of the requested digits. If at least
    // half of the requested digits have been achieved,
    // then break after the upcoming iteration.
    const bool break_after_this_iteration = (   (k > static_cast<std::int32_t>(4))
                                             && (fabs(1 - fabs(ak / bk)) < target_tolerance));

    ak_tmp  = ak;
    ak     += bk;
    ak     /= 2;

    if(break_after_this_iteration)
    {
      break;
    }

    bk *= ak_tmp;
    bk  = sqrt(bk);
  }

  // We are now finished with the AGM iteration for log(x).

  // Compute log(x) = {pi / [2 * AGM(1, 4 / 2^m)]} - (m * ln2)
  // Note at this time that (ak = bk) = AGM(...)

  // Retrieve the value of pi, divide by (2 * a) and subtract (m * ln2).
  const FloatingPointType result =
       boost::math::constants::pi<FloatingPointType>() / (ak * 2)
    - (boost::math::constants::ln_two<FloatingPointType>() * m);

  return ((b_negate == true) ? -result : result);
}

} } } // namespace boost::multiprecision::exercise_threading

template<typename FloatingPointType>
bool log_agm_concurrent(float& calculation_time)
{
  const std::size_t count = boost::multiprecision::exercise_threading::detail::primes().size();

  std::vector<FloatingPointType> log_results(count);
  std::vector<FloatingPointType> log_control(count);

  std::atomic_flag log_agm_lock = ATOMIC_FLAG_INIT;

  std::size_t concurrent_log_agm_count = 0U;

  const std::clock_t start = std::clock();

  boost::multiprecision::exercise_threading::detail::my_concurrency::parallel_for
  (
    std::size_t(0U),
    log_results.size(),
    [&log_results, &log_control, &concurrent_log_agm_count, &log_agm_lock](std::size_t i)
    {
      while(log_agm_lock.test_and_set()) { ; }
      const FloatingPointType dx = (FloatingPointType(1U) / (boost::multiprecision::exercise_threading::detail::primes()[i]));
      log_agm_lock.clear();

      const FloatingPointType  x = boost::math::constants::catalan<FloatingPointType>() + dx;

      const FloatingPointType lr = boost::multiprecision::exercise_threading::log(x);
      const FloatingPointType lc = boost::multiprecision::log(x);

      while(log_agm_lock.test_and_set()) { ; }

      log_results[i] = lr;
      log_control[i] = lc;

      ++concurrent_log_agm_count;

      if((concurrent_log_agm_count % 100U) == 0U)
      {
        std::cout << "log agm concurrent at index "
                  << concurrent_log_agm_count
                  << " of "
                  << log_results.size()
                  << ". Total processed so far: "
                  << std::fixed
                  << std::setprecision(1)
                  << (100.0F * float(concurrent_log_agm_count)) / float(log_results.size())
                  << "%."
                  << "\r";
      }

      log_agm_lock.clear();
    }
  );

  calculation_time = static_cast<float>(std::clock() - start) / static_cast<float>(CLOCKS_PER_SEC);

  std::cout << std::endl;

  std::cout << "Checking results concurrent: ";

  bool result_is_ok = true;

  const FloatingPointType tol = std::numeric_limits<FloatingPointType>::epsilon() * 1000000U;

  for(std::size_t i = 0U; i < log_results.size(); ++i)
  {
    using std::fabs;

    const FloatingPointType close_fraction = fabs(1 - (log_results[i] / log_control[i]));

    result_is_ok &= (close_fraction < tol);
  }

  std::cout << std::boolalpha << result_is_ok << std::endl;

  return result_is_ok;
}

template<typename FloatingPointType>
bool log_agm_sequential(float& calculation_time)
{
  const std::size_t count = boost::multiprecision::exercise_threading::detail::primes().size();

  std::vector<FloatingPointType> log_results(count);
  std::vector<FloatingPointType> log_control(count);

  const std::clock_t start = std::clock();

  for(std::size_t i = 0U; i < log_results.size(); ++i)
  {
    const std::size_t sequential_log_agm_count = i + 1U;

    const FloatingPointType dx = (FloatingPointType(1U) / (boost::multiprecision::exercise_threading::detail::primes()[i]));
    const FloatingPointType  x = boost::math::constants::catalan<FloatingPointType>() + dx;

    log_results[i] = boost::multiprecision::exercise_threading::log(x);
    log_control[i] = boost::multiprecision::log(x);

    if((sequential_log_agm_count % 100U) == 0U)
    {
      std::cout << "log agm sequential at index "
                << sequential_log_agm_count
                << " of "
                << log_results.size()
                << ". Total processed so far: "
                << std::fixed
                << std::setprecision(1)
                << (100.0F * float(sequential_log_agm_count)) / float(log_results.size())
                << "%."
                << "\r";
    }
  }

  calculation_time = static_cast<float>(std::clock() - start) / static_cast<float>(CLOCKS_PER_SEC);

  std::cout << std::endl;

  std::cout << "Checking results sequential: ";

  bool result_is_ok = true;

  const FloatingPointType tol = std::numeric_limits<FloatingPointType>::epsilon() * 1000000U;

  for(std::size_t i = 0U; i < log_results.size(); ++i)
  {
    using std::fabs;

    const FloatingPointType close_fraction = fabs(1 - (log_results[i] / log_control[i]));

    result_is_ok &= (close_fraction < tol);
  }

  std::cout << std::boolalpha << result_is_ok << std::endl;

  return result_is_ok;
}

int main()
{
  std::cout << "Calculating "
            << boost::multiprecision::exercise_threading::detail::primes().size()
            << " primes"
            << std::endl;

  float calculation_time_concurrent;
  const bool result_is_ok_concurrent = log_agm_concurrent<big_float_type>(calculation_time_concurrent);

  float calculation_time_sequential;
  const bool result_is_ok_sequential = log_agm_sequential<big_float_type>(calculation_time_sequential);

  std::cout << std::endl;

  std::cout << "result_is_ok_concurrent: "
            << std::boolalpha
            << result_is_ok_concurrent
            << ", calculation_time_concurrent: "
            << std::fixed
            << std::setprecision(1)
            << calculation_time_concurrent
            << "s"
            << std::endl;

  std::cout << "result_is_ok_sequential: "
            << std::boolalpha
            << result_is_ok_sequential
            << ", calculation_time_sequential: "
            << std::fixed
            << std::setprecision(1)
            << calculation_time_sequential
            << "s"
            << std::endl;
}
